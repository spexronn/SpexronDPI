#pragma once

#include <cstdint>

namespace SpexronDPI::Parsers {

#pragma pack(push, 1)

struct IPv4Header {
    uint8_t ihl : 4;       
    uint8_t version : 4;   
    uint8_t tos;           
    uint16_t totalLength;  
    uint16_t id;           
    uint16_t fragOffset;   
    uint8_t ttl;           
    uint8_t protocol;      
    uint16_t checksum;     
    uint32_t srcIp;        
    uint32_t dstIp;        
};

struct TcpHeader {
    uint16_t srcPort;      
    uint16_t dstPort;      
    uint32_t seqNum;       
    uint32_t ackNum;       
    uint8_t reserved1 : 4;
    uint8_t dataOffset : 4; 
    uint8_t flags;         
    uint16_t window;       
    uint16_t checksum;     
    uint16_t urgPtr;       
};

struct UdpHeader {
    uint16_t srcPort;
    uint16_t dstPort;
    uint16_t length;
    uint16_t checksum;
};

#pragma pack(pop) 

} 
