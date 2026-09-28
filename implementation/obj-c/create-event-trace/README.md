# Objective C: create-event-trace

Plots every job in `job.log` on its own row over time.
Box = waiting in queue, circle = in service, x = error. Color = queue.

```bash
./create-event-trace job.log out.png
./create-event-trace job.log out.png --title="Run 1"
```

## Build and Test

```bash
make build
make test
make clean
```
