/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Simulation");

/**
 * Retransmissions
**/
static void
ReTrace(Ptr<OutputStreamWrapper> stream, uint32_t oldValue, uint32_t newValue)
{   
    *stream->GetStream() << "Retr " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
}

/**
 * Sender
**/
static void
SendTrace(Ptr<OutputStreamWrapper> stream, SequenceNumber32 oldValue, SequenceNumber32 newValue)
{   
    *stream->GetStream() << "Send " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
}

/**
 * Recv
**/
static void
RecvTrace(Ptr<OutputStreamWrapper> stream, SequenceNumber32 oldValue, SequenceNumber32 newValue)
{   
    *stream->GetStream() << "Recv " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
}

/**
 * Set up TCP connection with trace.
*/
void 
traceHandler(Ptr<BulkSendApplication> app)
{

    // Socket.
    Ptr<TcpSocketBase> socket = DynamicCast<TcpSocketBase>(app->GetSocket());

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> fct = ascii.CreateFileStream("fct.tr");
    Ptr<OutputStreamWrapper> re = ascii.CreateFileStream("re.tr");

    // Add traces.
    bool ok = true;
    ok = socket->TraceConnectWithoutContext("HighestSequence", MakeBoundCallback(&SendTrace, fct));
    NS_ASSERT(ok);
    ok = socket->TraceConnectWithoutContext("HighestRxAck", MakeBoundCallback(&RecvTrace, fct));
    NS_ASSERT(ok);
    ok = socket->TraceConnectWithoutContext("RetransmissionsRemain", MakeBoundCallback(&ReTrace, re));
    NS_ASSERT(ok);

    // Finally connected.
    app->SetConnected(true);
}

/**
 * Send size number of bytes from the BulkSendApplication stored in apps.
*/
void
flowHandler(ApplicationContainer apps, uint32_t size)
{
    Ptr<BulkSendApplication> send = DynamicCast<BulkSendApplication>(apps.Get(0));
    NS_ASSERT(send->GetConnected());
    send->SetMaxBytes(size);
    send->SendData(send->GetLocal(), send->GetPeer());
}


int
main(int argc, char* argv[])
{
    uint32_t send_size = 100000;
    double error_rate = 0.00;
    uint8_t sack = 4;
    uint32_t data_retries = 5;
    uint32_t num_nodes = 2;

    //
    // Allow the user to override any of the defaults at
    // run-time, via command-line arguments
    //
    CommandLine cmd(__FILE__);
    cmd.AddValue("MaxBytes", "Flow Size", send_size);
    cmd.AddValue("ErrorRate", "Packet Drop Rate", error_rate);
    cmd.AddValue("Sack", "n-Sack", sack);
    cmd.AddValue("DataRetries", "TCP Max Retransmissions", data_retries);
    cmd.AddValue("Nodes", "Number of Nodes", num_nodes);
    cmd.Parse(argc, argv);

    uint64_t max_bytes = (uint32_t) send_size * 2; // set to 10,000 for actual simulation.

    Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(data_retries));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(50000000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(50000000));
    Config::SetDefault("ns3::TcpSocketBase::NSack", UintegerValue(sack));
    
    //
    // Explicitly nodes specified by num_nodes. // TODO integrate into fat-tree topology.
    //
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(num_nodes);

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point link required by the topology (shown above).
    //
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Gbps")); // TODO also run simulation at 40Gbps
    p2p.SetChannelAttribute("Delay", StringValue("1000ns"));

    // TODO datacenter packet loss is extremely low according to Kevin's paper. Most loss comes from
    // oversubscription.

    // Set error rate
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetStream(50);
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetRandomVariable(uv);
    em->SetRate(error_rate);
    em->SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
    p2p.SetDeviceAttribute("ReceiveErrorModel", PointerValue(em));

    NetDeviceContainer devices;
    devices = p2p.Install(nodes);

    //
    // Install the internet stack on the nodes
    //
    InternetStackHelper internet;
    internet.Install(nodes);

    //
    // We've got the "hardware" in place.  Now we need to add IP addresses.
    //
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interface = ipv4.Assign(devices);

    NS_LOG_INFO("Create Applications.");

    // Create a 2D matrix where maxtrix[i][j] represents an ApplicationContainer where the first Application in
    // the container is a BulkSendApplication sending to the second Application, which is a PacketSink.
    std::vector<std::vector<ApplicationContainer>> matrix(num_nodes, std::vector<ApplicationContainer>(num_nodes));
    for (uint32_t i = 0; i < num_nodes; i++) 
    {
        for (uint32_t j = 0; j < num_nodes; j++)
        {
            if (i != j)
            {
                ApplicationContainer apps;

                //
                // Create a BulkSendApplication and install it on node i.
                //
                uint16_t port = 9000 + i;

                BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(interface.GetAddress(j), port));
                source.SetAttribute("MaxBytes", UintegerValue(0));
                apps.Add(source.Install(nodes.Get(i)));

                // Create a callback, which indicates when the TCP connection succeeds, so can set
                // up traces.
                DynamicCast<BulkSendApplication>(apps.Get(0))->SetConnectCallback(&traceHandler);

                //
                // Create a PacketSinkApplication and install it on node 1
                //
                PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                apps.Add(sink.Install(nodes.Get(j)));
                apps.Start(Seconds(0.0));
                apps.Stop(Seconds(5000.0));

                matrix[i][j] = apps;
            }  
        }
    }


    //
    // Set up tracing on L2
    //
    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("l2.tr");
    p2p.EnableAscii(stream, devices.Get(0));


    //
    // The next block of code is for testing because I've been unable to read from
    // the flow file. See below for explanation of issue.
    //

    // Fake input for testing between 2 nodes and schedule them.
    std::vector<std::vector<std::string>> input;
    std::string input_data[] = {
        "1 0 3 100 50773 2.000227265",  
        "0 1 3 100 3929948 2.001185171",  
        "1 0 3 100 30071 2.002157443",  
        "1 0 3 100 676 2.003654232",  
        "0 1 3 100 76409 2.005367012",  
        "0 1 3 100 1649191 2.009730850",  
        "0 1 3 100 8993 2.009783566",  
        "1 0 3 100 15700100 2.011084367",  
        "1 0 3 100 20305770 2.012322672",  
        "1 0 3 100 59025 2.013073333"
    };
    for (const std::string& line : input_data) 
    {
        std::vector<std::string> params;
        std::istringstream iss(line);
        std::string token;
        while (std::getline(iss, token, ' ')) 
        {
            params.push_back(token);
        }
        Simulator::Schedule(Seconds(std::stod(params[5])), 
                            &flowHandler, 
                            matrix[std::stoi(params[0])][std::stoi(params[1])], 
                            std::stoi(params[4])); 
    }

    //
    // TODO Schedule flows based on file input. It's unable to read from the file because
    // it immediately hits an end of file?
    // 

    // std::ifstream file;
    // file.open("tmp_traffic.txt");
    // NS_ASSERT(!file.is_open());
    // NS_ASSERT(!file.eof());

    // std::string row;
    // while (std::getline(file, row))
    // {
    //     std::vector<std::string> params;
        
    //     std::istringstream iss(row);
    //     std::string value;

    //     while (std::getline(iss, value, ' ')) {
    //         params.push_back(value);
    //     }

    //     Simulator::Schedule(Seconds(std::stod(params[5])), 
    //                         &flowHandler, 
    //                         matrix[std::stoi(params[0])][std::stoi(params[1])], 
    //                         std::stoi(params[4])); 
    // }
    // file.close();

    //
    // Now, do the actual simulation.
    //
    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(5000.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    // View the total bytes received at each PacketSink.
    for (uint32_t i = 0; i < num_nodes; i++) 
    {
        for (uint32_t j = 0; j < num_nodes; j++)
        {
            if (i != j)
            {
                ApplicationContainer apps = matrix[i][j];
                Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(apps.Get(1));
                std::cout << "Total Bytes Received: " << sink1->GetTotalRx() << std::endl;
            }  
        }
    }

    return 0;
}