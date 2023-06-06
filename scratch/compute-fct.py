import csv
import math


NUM_NODES = 16

queues = NUM_NODES * [None] # (time, seqNum, size)
fct = [] # (node, size, time)

with open('scratch/traces/test_4sack_fct.tr', 'r') as trace, open('traffic_gen/small_traffic.txt') as flow:
    f = csv.reader(flow, delimiter=" ")
    start = next(f)

    t = csv.reader(trace, delimiter=" ")
    for row in t:

        t_node = int(row[0])
        t_status = row[1]
        t_time = int(row[2])
        t_seqNum = int(row[3])

        # Missed a scheduled flow. This is most likely because it was scheduled to start
        # before the previous flow had finished on that node.
        # We add the size of this flow to the previous flow and merge them into 1 big flow.
        while start and t_time > int(start[5].replace(".", "")) + 1:
            if queues[int(start[0])]: 
                queues[int(start[0])][-1] = queues[int(start[0])][-1][:2] + \
                                            (queues[int(start[0])][-1][2] + int(start[4]), "m")
            try: start = next(f)
            except StopIteration: start = None

        # The start of a scheduled flow.
        if start and t_node == int(start[0]) and t_status == 'Send' and \
            int(start[5].replace(".", "")) == t_time - 1:
            
            if not queues[t_node]: queues[t_node] = []
            queues[t_node].append((t_time, t_seqNum, int(start[4])))

            try: start = next(f)
            except StopIteration: start = None           

        # The end of a flow. We also must remove other flows from that node in the 
        # case that the ack was for 2 separate flows.
        elif t_status == 'Recv' and queues[t_node] and \
            t_seqNum >= (queues[t_node][0][1] + queues[t_node][0][2]) % 2**32:

            fct.append((t_node, t_seqNum - queues[t_node][0][1], t_time - queues[t_node][0][0]))
            while queues[t_node] and t_seqNum >= (queues[t_node][0][1] + queues[t_node][0][2]) % 2**32:
                queues[t_node].pop(0)

for i,v in enumerate(queues):
    print(f'{i}\t{v}')

print(f'{len(fct)} total FCT')