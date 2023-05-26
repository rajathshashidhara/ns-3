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

#include "ns3/point-to-point-fat-tree.h"

#include <fstream>
#include <string>
#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("Simulation");

/**
 * Retransmissions
**/
static void
ReTrace(Ptr<OutputStreamWrapper> stream, std::string context, uint32_t oldValue, uint32_t newValue)
{   
    *stream->GetStream() << context << " Retr " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
}

/**
 * Sender
**/
static void
SendTrace(Ptr<OutputStreamWrapper> stream, std::string context, SequenceNumber32 oldValue, SequenceNumber32 newValue)
{   
    *stream->GetStream() << context << " Send " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
}

/**
 * Recv
**/
static void
RecvTrace(Ptr<OutputStreamWrapper> stream, std::string context, SequenceNumber32 oldValue, SequenceNumber32 newValue)
{   
    *stream->GetStream() << context << " Recv " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
}

/**
 * Set up TCP connection with trace.
*/
void 
traceHandler(Ptr<BulkSendApplication> app, Ptr<OutputStreamWrapper> fct, Ptr<OutputStreamWrapper> re)
{
    uint32_t node = app->GetNode()->GetId();
    NS_LOG_INFO("Setting up trace on " << node);

    // Socket.
    Ptr<TcpSocketBase> socket = DynamicCast<TcpSocketBase>(app->GetSocket());

    // Add traces.
    bool ok = true;
    ok = socket->TraceConnectWithoutContext("HighestSequence", MakeBoundCallback(&SendTrace, fct, std::to_string(node)));
    NS_ASSERT(ok);
    ok = socket->TraceConnectWithoutContext("HighestRxAck", MakeBoundCallback(&RecvTrace, fct, std::to_string(node)));
    NS_ASSERT(ok);
    ok = socket->TraceConnectWithoutContext("RetransmissionsRemain", MakeBoundCallback(&ReTrace, re, std::to_string(node)));
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
    NS_LOG_INFO("Sending " << size);
    Ptr<BulkSendApplication> send = DynamicCast<BulkSendApplication>(apps.Get(0));
    NS_ASSERT(send->GetConnected());
    send->SendSize(size);
}


int
main(int argc, char* argv[])
{
    double error_rate = 0.00;
    uint8_t sack = 4;
    uint32_t data_retries = 5;
    uint32_t num_nodes = 16;

    //
    // Allow the user to override any of the defaults at
    // run-time, via command-line arguments
    //

    CommandLine cmd(__FILE__);
    cmd.AddValue("ErrorRate", "Packet Drop Rate", error_rate);
    cmd.AddValue("Sack", "n-Sack", sack);
    cmd.AddValue("DataRetries", "TCP Max Retransmissions", data_retries);
    cmd.AddValue("Pods", "Number of Pods", num_nodes);
    cmd.Parse(argc, argv);

    Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(data_retries));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(50000000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(50000000));
    Config::SetDefault("ns3::TcpSocketBase::NSack", UintegerValue(sack));

    double pods = cbrt(4 * num_nodes);
    if (fmod(pods, 2) != 0)
    {
        return 1;
    }
    uint32_t num_pods = static_cast<uint32_t>(pods);

    NS_LOG_INFO("Create channels.");

    //
    // Explicitly create the point-to-point link required by the topology (shown above).
    //

    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Gbps")); // TODO also run simulation at 40Gbps
    p2p.SetChannelAttribute("Delay", StringValue("1000ns"));

    // Set error rate
    Ptr<UniformRandomVariable> uv = CreateObject<UniformRandomVariable>();
    uv->SetStream(50);
    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetRandomVariable(uv);
    em->SetRate(error_rate);
    em->SetUnit(RateErrorModel::ERROR_UNIT_PACKET);
    p2p.SetDeviceAttribute("ReceiveErrorModel", PointerValue(em));

    //
    // Create a fat tree topology.
    //

    NS_LOG_INFO("Create fat tree topology.");

    PointToPointFatTreeHelper fattree(num_pods, p2p);

    // Install internet stack on the nodes
    InternetStackHelper internet;
    fattree.InstallStack(internet);

    // Assign IP addresses
    Ipv4Address ipv4("123.123.0.0");
    Ipv4Mask mask("/16");
    fattree.AssignIpv4Addresses(ipv4, mask);

    NS_LOG_INFO("Create Applications.");

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> fct = ascii.CreateFileStream("scratch/traces/fct.tr");
    Ptr<OutputStreamWrapper> re = ascii.CreateFileStream("scratch/traces/retr.tr");


    // Create a 2D matrix where maxtrix[i][j] = ApplicationContainer.
    // ApplicationContainer.Get(0) = BulkSendApplication on node i.
    // ApplicationContainer.Get(1) = PacketSink on node j.

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

                BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(fattree.GetServerIpv4Address(j), port));
                source.SetAttribute("MaxBytes", UintegerValue(0));
                apps.Add(source.Install(fattree.GetServerNode(i)));

                // Create a callback, which indicates when the TCP connection succeeds, so can set
                // up traces.
                Ptr<BulkSendApplication> app = DynamicCast<BulkSendApplication>(apps.Get(0));
                app->SetTraceStreams(fct, re);
                app->SetConnectCallback(&traceHandler);

                //
                // Create a PacketSinkApplication and install it on node j.
                //
                PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), port));
                apps.Add(sink.Install(fattree.GetServerNode(j)));
                apps.Start(Seconds(0.0));
                apps.Stop(Seconds(5000.0));

                matrix[i][j] = apps;
            }  
        }
    }

    NS_LOG_INFO("Use global routing.");
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    //
    // Read and schedule flows from traffic_gen.
    //

    std::ifstream file;
    file.open("traffic_gen/tmp_traffic.txt", std::ifstream::in);

    std::string row;
    while (std::getline(file, row))
    {
        std::vector<std::string> params;
        std::istringstream iss(row);
        std::string value;

        while (std::getline(iss, value, ' ')) {
            params.push_back(value);
        }

        Simulator::Schedule(Seconds(std::stod(params[5])), 
                            &flowHandler, 
                            matrix[std::stoi(params[0])][std::stoi(params[1])], 
                            std::stoi(params[4])); 
    }
    file.close();

    //
    // Now, do the actual simulation.
    //

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(Seconds(5000.0));
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    std::vector<uint64_t> nodes(num_nodes, 0);

    // View the total bytes received at each PacketSink.
    for (uint32_t i = 0; i < num_nodes; i++) 
    {
        for (uint32_t j = 0; j < num_nodes; j++)
        {
            if (i != j)
            {
                ApplicationContainer apps = matrix[i][j];
                Ptr<PacketSink> sink1 = DynamicCast<PacketSink>(apps.Get(1));
                nodes[j] += sink1->GetTotalRx();
            }  
        }
    }

    for (uint32_t i = 0; i < nodes.size(); i++) {
        std::cout << "Node " << std::to_string(i) << " Received " << std::to_string(nodes[i]) << " Total Bytes" << std::endl;
    }

    return 0;
}