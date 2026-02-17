#!/bin/bash

# Automated experiment runner for RPC accelerator evaluation
# Usage: ./run_experiments.sh [save_checkpoints|run_experiments]

set -e  # Exit on any error

# Configuration
OUTDIR_BASE="outdir/exps"
RESULT_DIR="$OUTDIR_BASE/result-dir/no-checkpoint-5k-post"
#RESULT_DIR="$OUTDIR_BASE/result-dir/latency-breakdown"
CHECKPOINT_DIR="$OUTDIR_BASE/checkpoint-dir/no-checkpoint-5k-post"
#CHECKPOINT_DIR="$OUTDIR_BASE/checkpoint-dir/latency-breakdown"
SIMOUT_DIR="$OUTDIR_BASE/simout-files/arcalis_vs_dagger_memcached"
#SIMOUT_DIR="$OUTDIR_BASE/simout-files/latency-breakdown"
DISK_IMAGE="disk_image/rpc_accelerator/rpc_accelerator.img"
KERNEL_PATH="hardware/linux-5.15.141/vmlinux"
CACHE_CONFIG="PrebuiltMesh6"

# Create output directories
mkdir -p "$RESULT_DIR" "$CHECKPOINT_DIR" "$SIMOUT_DIR"

# Experiment configurations
declare -A EXPERIMENTS=(
    #["unique-id"]="unique-id 200000 uniqueid_traces/dpdk_to_rpc.bin 200k"
    ["memcached-a"]="memcached 2010000 log_zf_200k_w10k_s005_k8_v8/dpdk_to_rpc.log 200k_10k_005"
    ["memcached-b"]="memcached 2010000 log_zf_200k_w10k_s050_k8_v8/dpdk_to_rpc.log 200k_10k_050"
    ["memcached-c"]="memcached 2010000 log_zf_200k_w10k_s005_k16_v32/dpdk_to_rpc.log 200k_10k_005"
    ["memcached-d"]="memcached 2010000 log_zf_200k_w10k_s050_k16_v32/dpdk_to_rpc.log 200k_10k_050"
    #["post-storage-a"]="post-storage 11000 poststorage_traces/dpdk_to_rpc.bin 10k_1k_03"
    #["post-storage-b"]="post-storage 11000 poststorage_traces/dpdk_to_rpc_10k_1k_01.bin 10k_1k_01"
    #["post-storage-c"]="post-storage 11000 poststorage_traces/dpdk_to_rpc_10k_1k_09.bin 10k_1k_09"
    #["post-storage-d"]="post-storage 6000 poststorage_traces/dpdk_to_rpc.bin 10k_1k_03"
    #["post-storage-e"]="post-storage 6000 poststorage_traces/dpdk_to_rpc_10k_1k_01.bin 10k_1k_01"
    #["post-storage-f"]="post-storage 6000 poststorage_traces/dpdk_to_rpc_10k_1k_09.bin 10k_1k_09"
)

# Function to run a single experiment
run_experiment() {
    local task=$1
    local use_accelerator=$2
    local iter=$3
    local trace_file=$4
    local size_label=$5
    
    local accel_suffix=""
    local accel_flag="false"
    
    if [ "$use_accelerator" = true ]; then
        accel_suffix="accl"
        accel_flag="true"
    else
        accel_suffix="no-accl"
        accel_flag="false"
    fi
    
    local exp_name="${task}-${accel_suffix}-${size_label}"
    
    echo "Starting experiment: $exp_name"
    
    hardware/gem5/build/latency-${task}/gem5.opt \
        --outdir="$RESULT_DIR/m5out-$exp_name" \
        experiment/rpc_accelerator/rpc_accelerator-test.py \
        --fast-forward-cpu-type=kvm \
        --cpu-type=o3 \
        --disk-image-path="$DISK_IMAGE" \
        --kernel-path="$KERNEL_PATH" \
        --cache-configuration="$CACHE_CONFIG" \
        --task="$task" \
        --use-cerebellum-engine="$accel_flag" \
        --num-threads=1 \
        --delay=1 \
        --use-translation-prefetch=false \
        --file="$trace_file" \
        --iter="$iter" \
        > "$SIMOUT_DIR/simout-$exp_name" 2>&1 &
    
    #experiment/rpc_accelerator/rpc_accelerator-test-restore-from-checkpoint.py \
    #--restore-from-checkpoint-path="$CHECKPOINT_DIR/$exp_name-checkpoint" \
    local pid=$!
    echo "Experiment $exp_name started with PID: $pid"
    echo "$pid" >> "$SIMOUT_DIR/running_pids.txt"
}

# Function to save checkpoints
save_checkpoint() {
    local task=$1
    local use_accelerator=$2
    local iter=$3
    local trace_file=$4
    local size_label=$5
    
    local accel_suffix=""
    local accel_flag="false"
    
    if [ "$use_accelerator" = true ]; then
        accel_suffix="accl"
        accel_flag="true"
    else
        accel_suffix="no-accl"
        accel_flag="false"
    fi
    
    local exp_name="${task}-${accel_suffix}-${size_label}"
    
    echo "Saving checkpoint: $exp_name"
    
    hardware/gem5/build/latency-${task}/gem5.opt \
        --outdir="$CHECKPOINT_DIR/m5out-$exp_name-checkpoint" \
        experiment/rpc_accelerator/rpc_accelerator-test-save-checkpoint.py \
        --fast-forward-cpu-type=kvm \
        --cpu-type=o3 \
        --disk-image-path="$DISK_IMAGE" \
        --kernel-path="$KERNEL_PATH" \
        --cache-configuration="$CACHE_CONFIG" \
        --task="$task" \
        --use-cerebellum-engine="$accel_flag" \
        --num-threads=1 \
        --delay=1 \
        --use-translation-prefetch=false \
        --save-checkpoint-path="$CHECKPOINT_DIR/$exp_name-checkpoint" \
        --file="$trace_file" \
        --iter="$iter" \
        > "$SIMOUT_DIR/simout-$exp_name-checkpoint" 2>&1 &
    
    local pid=$!
    echo "Checkpoint $exp_name started with PID: $pid"
    echo "$pid" >> "$SIMOUT_DIR/checkpoint_pids.txt"
}

# Function to run all experiments
run_all_experiments() {
    echo "Starting all experiments..."
    rm -f "$SIMOUT_DIR/running_pids.txt"
    
    for exp_key in "${!EXPERIMENTS[@]}"; do
        IFS=' ' read -r task iter trace_file size_label <<< "${EXPERIMENTS[$exp_key]}"
        
        # Run with accelerator
        run_experiment "$task" true "$iter" "$trace_file" "$size_label"
        
        # Run without accelerator
        #run_experiment "$task" false "$iter" "$trace_file" "$size_label"
    done
    
    echo "All experiments started. PIDs saved in $SIMOUT_DIR/running_pids.txt"
}

# Function to save all checkpoints
save_all_checkpoints() {
    echo "Starting checkpoint creation..."
    rm -f "$SIMOUT_DIR/checkpoint_pids.txt"
    
    for exp_key in "${!EXPERIMENTS[@]}"; do
        IFS=' ' read -r task iter trace_file size_label <<< "${EXPERIMENTS[$exp_key]}"
        
        # Save checkpoint with accelerator
        save_checkpoint "$task" true "$iter" "$trace_file" "$size_label"
        
        # Save checkpoint without accelerator
        save_checkpoint "$task" false "$iter" "$trace_file" "$size_label"
    done
    
    echo "All checkpoints started. PIDs saved in $SIMOUT_DIR/checkpoint_pids.txt"
}

# Function to check experiment status
check_status() {
    if [ -f "$SIMOUT_DIR/running_pids.txt" ]; then
        echo "Checking running experiments..."
        while read -r pid; do
            if kill -0 "$pid" 2>/dev/null; then
                echo "PID $pid is still running"
            else
                echo "PID $pid has finished"
            fi
        done < "$SIMOUT_DIR/running_pids.txt"
    else
        echo "No running experiments found"
    fi
}

# Function to kill all experiments
kill_all() {
    if [ -f "$SIMOUT_DIR/running_pids.txt" ]; then
        echo "Killing all running experiments..."
        while read -r pid; do
            if kill -0 "$pid" 2>/dev/null; then
                kill "$pid"
                echo "Killed PID $pid"
            fi
        done < "$SIMOUT_DIR/running_pids.txt"
        rm -f "$SIMOUT_DIR/running_pids.txt"
    fi
    
    if [ -f "$SIMOUT_DIR/checkpoint_pids.txt" ]; then
        echo "Killing all checkpoint processes..."
        while read -r pid; do
            if kill -0 "$pid" 2>/dev/null; then
                kill "$pid"
                echo "Killed checkpoint PID $pid"
            fi
        done < "$SIMOUT_DIR/checkpoint_pids.txt"
        rm -f "$SIMOUT_DIR/checkpoint_pids.txt"
    fi
}

# Function to show results summary
show_results() {
    echo "Experiment Results Summary:"
    echo "=========================="
    
    for exp_key in "${!EXPERIMENTS[@]}"; do
        IFS=' ' read -r task iter trace_file size_label <<< "${EXPERIMENTS[$exp_key]}"
        
        echo -e "\n$task ($size_label iterations):"
        
        accel_log="$SIMOUT_DIR/simout-$task-accl-$size_label"
        no_accel_log="$SIMOUT_DIR/simout-$task-no-accl-$size_label"
        
        if [ -f "$accel_log" ]; then
            echo "  With accelerator: $(tail -1 "$accel_log" 2>/dev/null || echo "Still running/failed")"
        fi
        
        if [ -f "$no_accel_log" ]; then
            echo "  Without accelerator: $(tail -1 "$no_accel_log" 2>/dev/null || echo "Still running/failed")"
        fi
    done
}

# Main script logic
case "${1:-run_experiments}" in
    "save_checkpoints")
        save_all_checkpoints
        ;;
    "run_experiments")
        run_all_experiments
        ;;
    "status")
        check_status
        ;;
    "kill")
        kill_all
        ;;
    "results")
        show_results
        ;;
    "help")
        echo "Usage: $0 [save_checkpoints|run_experiments|status|kill|results|help]"
        echo "  save_checkpoints: Create checkpoints for all experiments"
        echo "  run_experiments:  Run all experiments (default)"
        echo "  status:          Check status of running experiments"
        echo "  kill:            Kill all running experiments"
        echo "  results:         Show results summary"
        echo "  help:            Show this help message"
        ;;
    *)
        echo "Unknown command: $1"
        echo "Use '$0 help' for usage information"
        exit 1
        ;;
esac
