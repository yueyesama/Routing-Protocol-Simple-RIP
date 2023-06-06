#include <stdio.h>
#include <stdlib.h>
#include "routing_srip.h"
#include "network_ip.h"

void SripInit(Node* node, SripData** sripPtr, const NodeInput* nodeInput, int interfaceIndex)
{
	int i; 
	BOOL retVal;
	Message* newMsg;
	SripData* srip;

	// 确认节点恰好只有一个无线接口
	if (MAC_IsWiredNetwork(node, interfaceIndex))
		ERROR_ReportError("SRIP supports only wireless interfaces");
	if (node->numberInterfaces > 1)
		ERROR_ReportError("SRIP only supports one interface of node");

	// 给每个节点的本地srip变量分配内存
	srip = (SripData*) MEM_malloc(sizeof(SripData));  
	// 将srip数据存在本地，可通过NetworkIpGetRoutingProtocol()获取本地srip变量的指针
	(*sripPtr) = srip; 

	// 从config文件读取SRIP间隔参数，如果没读到，设为默认值
	IO_ReadTime(node->nodeId, ANY_ADDRESS, nodeInput, "SRIP-UPDATE-INTERVAL", &retVal, &(srip->updateInterval));
	if (retVal == FALSE) {
		srip->updateInterval = DEFAULT_UPDATEINTERVAL;
	}

	// 初始化路由表
	srip->routingTable = (SripTableEntry*) MEM_malloc(sizeof(SripTableEntry) * (node->numNodes + 1));
	for (i = 1; i <= node->numNodes; i++)
	{
		srip->routingTable[i].destination = i;
		srip->routingTable[i].nextHop = INVALID_ADDRESS;
		srip->routingTable[i].distance = SRIP_INFINITY;
	}
	srip->routingTable[node->nodeId].nextHop = node->nodeId;
	srip->routingTable[node->nodeId].distance = 0;
	
	// 初始化统计数据
	srip->numRouteUpdatesBroadcast = 0;

	// 向IP注册srip的RouterFunction
	NetworkIpSetRouterFunction(node, &SripRouterFunction, interfaceIndex);

	// 安排定时器以启动协议,为网络层的srip协议安排了MSG_NETWORK_RTBroadcastAlarm类型的网络层事件，在当前模拟时间延迟delay后发生
	newMsg = MESSAGE_Alloc(node, NETWORK_LAYER, ROUTING_PROTOCOL_SRIP, MSG_NETWORK_RTBroadcastAlarm);
	RandomSeed startupSeed;
	RANDOM_SetSeed(startupSeed, node->globalSeed, node->nodeId, ROUTING_PROTOCOL_SRIP, interfaceIndex);
	clocktype delay = RANDOM_nrand(startupSeed) % srip->updateInterval;
	MESSAGE_Send(node, newMsg, delay);
}

// 事件处理函数：当网络层事件发生后，exata内核响应事件调用该函数，并将事件内容作为参数传入
void SripHandleProtocolEvent(Node* node, Message* msg)
{
	int i, numEntries = 0, pktSize;
	Message* newMsg; 
	char* pktPtr;
	
	// 从本地读取SRIP数据
	SripData* srip = (SripData*) 
		NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_SRIP);

	// 计算满足要求的路由表行数numEntries
	for (i = 1; i <= node->numNodes; i++)
	{   
		if (srip->routingTable[i].distance < SRIP_INFINITY)
			numEntries++;		
	}

	// 预分配要发送的消息newMsg -> 内容：本节点有效路由表行数（一个int数据）+ 本节点有效路由表
	newMsg = MESSAGE_Alloc(node, 0, 0, 0);
	pktSize = sizeof(int) + sizeof(SripTableEntry) * numEntries; 
	MESSAGE_PacketAlloc(node, newMsg, pktSize, TRACE_ANY_PROTOCOL);
	
	// 赋值newMsg（Message类的packet字段用于存放消息的具体内容）
	pktPtr = newMsg->packet;
	memcpy(pktPtr, &numEntries, sizeof(int));
	pktPtr += sizeof(int);
	for (i = 1; i <= node->numNodes; i++)
	{    
		if (srip->routingTable[i].distance < SRIP_INFINITY) 
		{
			memcpy(pktPtr, &(srip->routingTable[i]), sizeof(SripTableEntry));
			pktPtr += sizeof(SripTableEntry);
		}
	}

	// 往Mac层发出信令消息，添加的网络层头中含有srip协议信息
	NetworkIpSendRawMessageToMacLayer(node, newMsg, node->nodeId, 
		ANY_DEST, 0, IPPROTO_SRIP, 1, DEFAULT_INTERFACE, ANY_DEST);

	// 更新统计数据
	srip->numRouteUpdatesBroadcast++;

	// 将信令消息发送后，重新安排新的网络层事件，在延迟updateInterval之后发生
	newMsg = MESSAGE_Alloc(node, NETWORK_LAYER, ROUTING_PROTOCOL_SRIP, MSG_NETWORK_RTBroadcastAlarm);
	MESSAGE_Send(node, newMsg, srip->updateInterval);
	// 将旧的网络层事件msg释放
	MESSAGE_Free(node, msg);
}

// 数据包处理函数
void SripHandleProtocolPacket(Node* node, Message* msg, NodeAddress sourceAddress)
{
	SripData* srip = (SripData*) 
		NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_SRIP);
	int i, numEntries;
	char* pktPtr;
	SripTableEntry entry;

	pktPtr = msg->packet;

	// 解析收到包中的路由表项条数
	memcpy(&numEntries, pktPtr, sizeof(int));
	pktPtr += sizeof(int);

	// 解析收到包中的路由表项，当本地对应的路由表项的距离大于收到路由表项的距离时，进行修改。完毕后将指针偏移到下一项
	for (i = 0; i < numEntries; i++)
	{
		memcpy(&entry, pktPtr, sizeof(SripTableEntry));
		entry.distance++;
		if (entry.distance < srip->routingTable[entry.destination].distance)
		{
			srip->routingTable[entry.destination].distance = entry.distance;
			srip->routingTable[entry.destination].nextHop = sourceAddress;
		}
		pktPtr += sizeof(SripTableEntry);
	}
	MESSAGE_Free(node, msg);
}

// SripRouterFunction对非SRIP包且非本节点自己发来的包进行处理
void SripRouterFunction(Node* node, Message* msg, NodeAddress destAddr, NodeAddress previousHopAddress, BOOL* packetWasRouted)
{
	int i;
	int dst;
	NodeAddress nextHop;

	// 通过IpHeaderType获取IP数据包包头，根据包头进行判断
	IpHeaderType *ipHeader = (IpHeaderType*) msg->packet;
	SripData* srip = (SripData*) 
		NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_SRIP);

	dst = MAPPING_GetNodeIdFromInterfaceAddress(node, ipHeader->ip_dst);
	nextHop = MAPPING_GetDefaultInterfaceAddressFromNodeId(node, srip->routingTable[dst].nextHop);

	if(ipHeader->ip_p == IPPROTO_SRIP || NetworkIpIsMyIP(node, ipHeader->ip_dst))
	{
		*packetWasRouted = FALSE;
		return;
	}
	// 对符合条件的包，查询对应的路由表，将其传送入下一跳
	if (srip->routingTable[dst].distance < SRIP_INFINITY)
	{
		*packetWasRouted = TRUE;
		NetworkIpSendPacketToMacLayer(node, msg, DEFAULT_INTERFACE, nextHop);
	}
}

// 在Finalize函数中打印出最后的路由表，以及各种统计数据
void SripFinalize(Node *node)
{   
	char buf[MAX_STRING_LENGTH];
	SripData* srip = (SripData*)NetworkIpGetRoutingProtocol(node, ROUTING_PROTOCOL_SRIP);
	sprintf(buf, "Number of Route Updates Broadcast = %u",srip->numRouteUpdatesBroadcast);
	IO_PrintStat(node, "Network", "SRIP", ANY_DEST, -1, buf);

	// 打印每个节点的路由表
	printf("Current node is %u\n", node->nodeId);
	printf("destination    nextHop    distance\n");
	for (int i = 1; i <= node->numNodes; i++)
	{
		printf("     %-12u%-12u%-12u",
			srip->routingTable[i].destination, srip->routingTable[i].nextHop, srip->routingTable[i].distance);
		printf("\n");
	}
}