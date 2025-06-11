// DPDKResources.h
#ifndef _THRIFT_TRANSPORT_DPDK_RESOURCES_H_
#define _THRIFT_TRANSPORT_DPDK_RESOURCES_H_

#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_malloc.h>

namespace apache {
namespace thrift {
namespace transport {

struct DPDKResources {
    uint16_t portId;
    struct rte_mempool* mbufPool;
    struct rte_eth_dev_info devInfo;
    struct rte_eth_conf portConf;
    void* rxQueue;
    bool isInitialized;
    
    DPDKResources() 
        : portId(0)
        , mbufPool(nullptr)
        , isInitialized(false) {
        memset(&devInfo, 0, sizeof(devInfo));
        memset(&portConf, 0, sizeof(portConf));
    }

};

}}} // apache::thrift::transport

#endif
