#ifndef _SRIP_H_
#define _SRIP_H_

#include "api.h"

//定义SRIP最大可达跳数
#define SRIP_INFINITY 16
#define DEFAULT_UPDATEINTERVAL 1 * SECOND

//定义srip路由表结构
typedef struct srip_table_entry
{
	NodeAddress destination;
	NodeAddress nextHop;
	int distance;
} SripTableEntry;

//定义SRIP协议所包含的全局数据
typedef struct srip_str
{
	clocktype updateInterval;
	SripTableEntry* routingTable;

	/* statistic */
	int numRouteUpdatesBroadcast;
} SripData;


void SripInit(Node* node, SripData** sripPtr, const NodeInput* nodeInput, int interfaceIndex);
void SripHandleProtocolEvent(Node* node, Message* msg);
void SripHandleProtocolPacket(Node* node, Message* msg,NodeAddress sourceAddress);
void SripFinalize(Node *node);
void SripRouterFunction(Node* node, Message* msg, NodeAddress destAddr,NodeAddress previousHopAddress, BOOL* packetWasRouted);
#endif
