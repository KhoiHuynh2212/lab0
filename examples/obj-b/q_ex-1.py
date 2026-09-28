#! /usr/bin/env python3
#
# A tiny simulator for a single queue.
# - richard.m.veras@ou.edu
#

# Each job is:
# (job ID, arrival time, service time)
jobs = [("a",1,2), # Arrives at 1, needs 2 service steps.
        ("b",2,1), # Arrives at 2, needs 1 service step.
        ("c",3,2), # Arrives at 3, needs 2 service steps.
        ("d",8,2), # Arrives at 8, needs 2 service steps.
        ("e",8,3), # Also arrives at 8, needs 3 service steps.
        ("f",9,2)] # Arrives at 9, needs 2 service steps.

# Rough service timeline from the original example.
# Notice that b arrives at 2 but does not start until a finishes at 3.
#
# |0 |1 |2 |3 |4 |5 |6 |7 |8 |9 |10|11|12
#     Ax_x_|
#           Bx|
#              Cx_x_|
#                          Dx|

clock=0  # Current simulation timestep.
limit=20 # Stop when clock reaches 20.

work_queue=list()    # Jobs that arrived and are waiting.
jobs_complete=list() # Jobs that finished.
job_in_service=None  # Current server job; None means idle.
service_time_left=0  # Remaining service time for the current job.

# Run one timestep at a time.
while clock < limit:

    # 1. If a job is already in service, perform one timestep of work.
    if job_in_service != None:
        service_time_left -= 1
        print("{c}: {j} has {t} time left".format(
            c=clock,
            j=job_in_service,
            t=service_time_left
        ));

    # 2. If the current job has finished, record it and free the server.
    if job_in_service != None and service_time_left == 0:
        print("{c}: {j} complete".format(
            c=clock,
            j=job_in_service
        ));

        jobs_complete.append((job_in_service,clock,0))
        job_in_service = None

    # 3. Add every job whose arrival time is the current clock.
    #   A while loop allows multiple jobs to arrive at the same time.
    while len(jobs) and jobs[0][1] == clock:
        j = jobs.pop(0)        # Remove job from future arrivals.
        work_queue.append(j)   # Add it to the back of the queue.
        print("{c}: {j} added to queue".format(
            c=clock,
            j=j
        ));

    # 4. If the server is free, take the first waiting job.
    if job_in_service == None and len(work_queue)>0:
        j = work_queue.pop(0)   # FIFO: remove from the front.
        job_in_service = j[0]   # Store the job ID.
        service_time_left = j[2]  # Load its service duration.

        print("{c}: {j} in service".format(
            c=clock,
            j=j
        ));

    # 5. Show waiting jobs as [id] and the server job as (id)->.
    print(''.join(
        ["[{id}]".format(id=j[0]) for j in work_queue]
        + ["({id})->".format(id=job_in_service)]
    ))

    # 6. Advance to the next timestep.
    clock+=1

# Print all jobs that completed.
print(jobs_complete)