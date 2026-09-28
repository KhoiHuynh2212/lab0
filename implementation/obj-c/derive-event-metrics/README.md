# Objective C: derive-event-metrics

Reads `job.log` and writes one row per job visit to a queue.

```bash
./derive-event-metrics job.log emetric.csv
```

Output columns:

```csv
job-id,queue-sys-id,arrival-time,time-in-queue,time-in-service,response-time,turn-around-time
```

- `response-time`: arrival until first `ENTERS-SERVER`
- `turn-around-time`: arrival until `DEPART-VIA` or `ERROR`
- `time-in-queue`: turn-around-time minus time-in-service

A cell is left empty if the visit never got that far (dropped job, run hit its time limit).

## Build and Test

```bash
make build
make test
make clean
```
