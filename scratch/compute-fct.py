import csv
import math


NUM_NODES = 16

queues = NUM_NODES * [None] # (time, seqNum, size)
fct = []

with open('scratch/traces/test_1sack_fct.tr', 'r') as trace, open('traffic_gen/small_traffic.txt') as flow:
    f = csv.reader(flow, delimiter=" ")
    start = next(f)

    t = csv.reader(trace, delimiter=" ")
    for row in t:
        if row[0] == start[0] and row[1] == 'Send' and int(start[5].replace(".", "")) == int(row[2]) + 1 and \
         (not queues[int(row[0])] or int(row[3]) >= (queues[int(row[0])][0][1] + queues[int(row[0])][0][2]) % 2**32):
            
            if not queues[int(row[0])]: queues[int(row[0])] = []
            queues[int(row[0])].append((int(row[2]), int(row[3]), int(start[4])))
            start = next(f, start)

        elif row[1] == 'Recv' and queues[int(row[0])] and int(row[3]) >= (queues[int(row[0])][0][1] + queues[int(row[0])][0][2]) % 2**32:
            fct.append((int(row[3]) - queues[int(row[0])][0][1], int(row[2]) - queues[int(row[0])][0][0]))
            while queues[int(row[0])] and int(row[3]) >= (queues[int(row[0])][0][1] + queues[int(row[0])][0][2]) % 2**32:
                queues[int(row[0])].pop(0)
    print(start)

print(queues)
print(fct)




# with open('mean.tr', 'a') as mean:
#     mean.write(f'\t{sum(fct) / len(fct)}\n')

# with open('tail.tr', 'a') as mean:
#     mean.write(f'\t{max(fct)}\n')