#!/bin/bash

# Usage check
if [ "$#" -lt 2 ] || [ "$#" -gt 3 ]; then
    echo "Usage: $0 [kernel|dpdk] [duration_sec] [uarch|hotspots (optional)]"
    exit 1
fi

MODE=$1
DURATION=$2
ANALYSIS=${3:-uarch}  # default to 'uarch' if not provided
OUTDIR=vtune_usertimeline_results

# Select binary and tag
if [ "$MODE" == "kernel" ]; then
    BIN="./usertimeline_kernel_server"
    TAG="kernel"
elif [ "$MODE" == "dpdk" ]; then
    BIN="./usertimeline_dpdk_server"
    TAG="dpdk"
else
    echo "Invalid mode: $MODE. Use 'kernel' or 'dpdk'."
    exit 1
fi

# Validate analysis type
if [ "$ANALYSIS" != "uarch" ] && [ "$ANALYSIS" != "hotspots" ]; then
    echo "Invalid analysis type: $ANALYSIS. Use 'uarch' or 'hotspots'."
    exit 1
fi

# Create output directory
sudo mkdir -p $OUTDIR
sudo chmod -R 777 $OUTDIR

echo "Launching $BIN pinned to core 1..."

# Start server pinned to core 1
sudo taskset -c 1 $BIN &
APP_PID=$!

# Give it a second to start
sleep 2

# Set VTune collection type
if [ "$ANALYSIS" == "uarch" ]; then
    COLLECTION="uarch-exploration"
    SUFFIX="uarch"
else
    COLLECTION="hotspots"
    SUFFIX="hotspots"
fi

echo "Profiling PID $APP_PID with VTune [$COLLECTION] for $DURATION seconds..."
sudo /opt/intel/oneapi/vtune/2025.4/bin64/vtune -collect $COLLECTION \
      -duration $DURATION \
      -target-pid $APP_PID \
      -result-dir $OUTDIR/usertimeline_${TAG}.${SUFFIX} \
      --quiet

# Kill the server after profiling
kill -9 $APP_PID
sleep 1

# Export VTune summary
sudo /opt/intel/oneapi/vtune/2025.4/bin64/vtune -report summary \
      -result-dir $OUTDIR/usertimeline_${TAG}.${SUFFIX} \
      > $OUTDIR/summary_${TAG}_${SUFFIX}.txt

echo "Done. Results in: $OUTDIR/usertimeline_${TAG}.${SUFFIX}"
echo "Summary report:  $OUTDIR/summary_${TAG}_${SUFFIX}.txt"

