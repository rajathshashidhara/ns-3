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
 *
 */

// Network topology
//
//       n0 ----------- n1
//            500 Kbps
//             5 ms
//
// - Flow from n0 to n1 using SendBulkApplication.
// - Tracing of queues and packet receptions to file "tcp-bulk-send.tr"
//   and pcap tracing available when tracing is turned on.

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TcpServer");

// /**
//  * Sender
// **/
// static void
// SendTrace(Ptr<OutputStreamWrapper> stream, SequenceNumber32 oldValue, SequenceNumber32 newValue)
// {   
//     *stream->GetStream() << " " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
// }

// /**
//  * Recv
// **/
// static void
// RecvTrace(Ptr<OutputStreamWrapper> stream, SequenceNumber32 oldValue, SequenceNumber32 newValue)
// {   
//     *stream->GetStream() << " " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
// }

// /**
//  * Retransmissions
// **/
// static void
// ReTrace(Time oldValue, Time newValue)
// {   
//     std::cout << " " << std::to_string(Simulator::Now().GetSeconds()) << " " << newValue << std::endl;
// }

int
main(int argc, char* argv[])
{
    // TODO track number of retransmissions

    uint32_t send_size = 1000000000;
    double error_rate = 0.1;

    // Allow the user to override any of the defaults and the above
    // Config::SetDefault()s at run-time, via command-line arguments
    CommandLine cmd(__FILE__);
    cmd.AddValue("MaxBytes", "Flow Size", send_size);
    cmd.AddValue("ErrorRate", "Packet Drop Rate", error_rate);
    cmd.Parse(argc, argv);

    uint32_t max_bytes = send_size * 10000;

    // Here, we will create N nodes in a star.
    NS_LOG_INFO("Create nodes.");
    NodeContainer nodes;
    nodes.Create(2);

    // We create the channels first without any IP addressing information
    NS_LOG_INFO("Create channels.");
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("5000ns"));

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

    AsciiTraceHelper ascii;
    Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("tcp.tr");
    p2p.EnableAscii(stream, devices.Get(1));

    // Install network stacks on the nodes
    InternetStackHelper internet;
    internet.Install(nodes);

    // Later, we add IP addresses.
    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i = ipv4.Assign(devices);

    // Turn on global static routing
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    uint16_t port = 9;
    Address anyAddress = InetSocketAddress(Ipv4Address::GetAny(), port);
    Ptr<Socket> socket = Socket::CreateSocket(nodes.Get(1), TcpSocketFactory::GetTypeId());
    socket->Bind(anyAddress);
    socket->Connect(InetSocketAddress(i.GetAddress(1), port));

    //
    // Create a SendBulkApplication and install it on node 0.
    //
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(i.GetAddress(1), port));
    // Set the amount of data to send in bytes.  Zero is unlimited.
    source.SetAttribute("MaxBytes", UintegerValue(max_bytes));
    source.SetAttribute("SendSize", UintegerValue(send_size));
    ApplicationContainer sourceApps = source.Install(nodes.Get(0));
    DynamicCast<BulkSendApplication>(sourceApps.Get(0))->SetSocket(socket);
    sourceApps.Start(Seconds(0.0));
    sourceApps.Stop(Seconds(10.0));
    

    // AsciiTraceHelper ascii;
    // Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("fct.tr");
    // ns3TcpSocket->TraceConnectWithoutContext("HighestSequence", MakeBoundCallback(&SendTrace, stream));
    // ns3TcpSocket->TraceConnectWithoutContext("HighestRxAck", MakeBoundCallback(&RecvTrace, stream));
    // // TODO install trace on m_dataRetrCount in ns3TcpSocket to trace retransmissions
    // // TODO "Since the maximum blocks that fits into a TCP header are 4, there's no point on maintaining the others."

    // // A SACK option that specifies n blocks will have a length of 8*n+2
    // // bytes, so the 40 bytes available for TCP options can specify a
    // // maximum of 4 blocks.  It is expected that SACK will often be used in
    // // conjunction with the Timestamp option used for RTTM [Jacobson92],
    // // which takes an additional 10 bytes (plus two bytes of padding); thus
    // // a maximum of 3 SACK blocks will be allowed in this case.

    // // Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("retransmission.tr");

    //
    // Create a PacketSinkApplication and install it on node 1
    //
    PacketSinkHelper sink("ns3::TcpSocketFactory", anyAddress);
    ApplicationContainer sinkApps = sink.Install(nodes.Get(1));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(10.0));

    NS_LOG_INFO("Run Simulation.");
    Simulator::Run();
    Simulator::Destroy();
    NS_LOG_INFO("Done.");

    return 0;
}
