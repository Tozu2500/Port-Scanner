#pragma once
#include <unordered_map>
#include <string>

inline const std::unordered_map<int, std::string>& getServiceMap() {
    static const std::unordered_map<int, std::string> services = {
        {20,    "FTP-Data"},
        {21,    "FTP"},
        {22,    "SSH"},
        {23,    "Telnet"},
        {25,    "SMTP"},
        {53,    "DNS"},
        {67,    "DHCP"},
        {68,    "DHCP-Client"},
        {69,    "TFTP"},
        {80,    "HTTP"},
        {110,   "POP3"},
        {119,   "NNTP"},
        {123,   "NTP"},
        {135,   "RPC"},
        {137,   "NetBIOS-NS"},
        {138,   "NetBIOS-DGM"},
        {139,   "NetBIOS-SSN"},
        {143,   "IMAP"},
        {161,   "SNMP"},
        {162,   "SNMP-Trap"},
        {179,   "BGP"},
        {194,   "IRC"},
        {389,   "LDAP"},
        {443,   "HTTPS"},
        {445,   "SMB"},
        {465,   "SMTPS"},
        {500,   "IKE"},
        {514,   "Syslog"},
        {515,   "LPD"},
        {587,   "SMTP-Sub"},
        {631,   "IPP"},
        {636,   "LDAPS"},
        {993,   "IMAPS"},
        {995,   "POP3S"},
        {1080,  "SOCKS"},
        {1194,  "OpenVPN"},
        {1433,  "MSSQL"},
        {1521,  "Oracle-DB"},
        {1723,  "PPTP"},
        {2049,  "NFS"},
        {2181,  "Zookeeper"},
        {2375,  "Docker"},
        {2376,  "Docker-TLS"},
        {3000,  "Node/Dev"},
        {3306,  "MySQL"},
        {3389,  "RDP"},
        {4369,  "EPMD"},
        {5000,  "Flask/Dev"},
        {5432,  "PostgreSQL"},
        {5672,  "AMQP"},
        {5900,  "VNC"},
        {5901,  "VNC-1"},
        {6379,  "Redis"},
        {6443,  "K8s-API"},
        {7001,  "WebLogic"},
        {8080,  "HTTP-Alt"},
        {8443,  "HTTPS-Alt"},
        {8888,  "Jupyter"},
        {9000,  "SonarQube"},
        {9090,  "Prometheus"},
        {9092,  "Kafka"},
        {9200,  "Elasticsearch"},
        {9300,  "Elasticsearch-N"},
        {11211, "Memcached"},
        {15672, "RabbitMQ-Mgmt"},
        {27017, "MongoDB"},
        {27018, "MongoDB-Shard"},
        {50000, "DB2"},
    };
    
    return services;
}

inline std::string lookupService(int port) {
    const auto& m = getServiceMap();
    auto it = m.find(port);
    return it != m.end() ? it->second : "Unknown";
}