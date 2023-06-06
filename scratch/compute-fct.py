import csv
import math


NUM_NODES = 16

queues = NUM_NODES * [None] # (time, seqNum, size)
fct = []

with open('scratch/traces/test_4sack_fct.tr', 'r') as trace, open('traffic_gen/small_traffic.txt') as flow:
    f = csv.reader(flow, delimiter=" ")
    start = next(f)

    t = csv.reader(trace, delimiter=" ")
    for row in t:

        # Missed a scheduled flow. This is most likely because it was scheduled to start
        # before the previous flow had finished on that node.
        while start and int(row[2]) > int(start[5].replace(".", "")) + 1:
            if queues[int(start[0])]: 
                queues[int(start[0])][-1] = queues[int(start[0])][-1][:2] + (queues[int(start[0])][-1][2] + int(start[4]), "m")
            try: start = next(f)
            except StopIteration: start = None

        # The start of a flow.
        if start and row[0] == start[0] and row[1] == 'Send' and int(start[5].replace(".", "")) == int(row[2]) - 1:
            
            if not queues[int(row[0])]: queues[int(row[0])] = []
            queues[int(row[0])].append((int(row[2]), int(row[3]), int(start[4])))

            try: start = next(f)
            except StopIteration: start = None           

        # The end of a flow. We also must remove other flows from that node in the 
        # case that the ack was for 2 separate flows.
        elif row[1] == 'Recv' and queues[int(row[0])] and \
            int(row[3]) >= (queues[int(row[0])][0][1] + queues[int(row[0])][0][2]) % 2**32:

            fct.append((row[0], int(row[3]) - queues[int(row[0])][0][1], int(row[2]) - queues[int(row[0])][0][0]))
            while queues[int(row[0])] and int(row[3]) >= (queues[int(row[0])][0][1] + queues[int(row[0])][0][2]) % 2**32:
                queues[int(row[0])].pop(0)

for i,v in enumerate(queues):
    print(f'{i}\t{v}')

print(len(fct))