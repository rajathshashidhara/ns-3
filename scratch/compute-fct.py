# TODO need to rewrite this based on changes from scheduling flows from file.

import csv

SENT_BYTES = 100000

queue = [(0, 1.0)] # (seqNum, time)
fct = []

with open(f'fct.tr', 'r') as f:
    reader = csv.reader(f, delimiter=" ")
    for row in reader:
        if int(row[2]) >= queue[0][0] + SENT_BYTES:
            if row[0] == 'Send': queue.append((int(row[2]), float(row[1])))
            elif row[0] == 'Recv':
                fct.append(float(row[1]) - queue[0][1])
                print(row)
                print(queue)
                print(fct)
                while queue and queue[0][0] <= int(row[2]): 
                    queue.pop(0)



# with open('mean.tr', 'a') as mean:
#     mean.write(f'\t{sum(fct) / len(fct)}\n')

# with open('tail.tr', 'a') as mean:
#     mean.write(f'\t{max(fct)}\n')