# Docker Setup for AncestralClust

This directory contains Docker configuration to run AncestralClust in an Ubuntu 20.04 environment.

## Quick Start

1. **Build the Docker image:**
   ```bash
   docker-compose build
   ```

2. **Run AncestralClust with help:**
   ```bash
   docker-compose run --rm ancestralclust-run --help
   ```

3. **Run AncestralClust with your data:**
   ```bash
   # Place your FASTA files in the ./data directory
   docker-compose run --rm ancestralclust-run -i /data/sequences.fasta -r 500 -b 10
   ```

## Docker Services

### `ancestralclust`
Interactive development container with shell access:
```bash
docker-compose run --rm ancestralclust
```

### `ancestralclust-run`
Direct execution of ancestralclust binary:
```bash
docker-compose run --rm ancestralclust-run [OPTIONS]
```

## Data Directory

- Place your input FASTA files in the `./data/` directory
- Output files will be written to the same directory
- The `./data/` directory is mounted as `/data` inside the container

## Example Usage

```bash
# Create a data directory (if not exists)
mkdir -p data

# Copy your FASTA file to data directory
cp my_sequences.fasta data/

# Run clustering with 500 initial sequences and 10 clusters
docker-compose run --rm ancestralclust-run \
  -i /data/my_sequences.fasta \
  -r 500 \
  -b 10 \
  -d /data \
  -f

# Output will be in the data/ directory
```

## Requirements

- Docker
- Docker Compose
- Input FASTA files with sequences on single lines (no line breaks)
- Sequences should contain only A, G, C, T, N (no ambiguous nucleotides or gaps)

## Build Details

- Base image: Ubuntu 20.04
- Compiler: GCC with OpenMP support
- Libraries: kalign3, WFA2, needleman-wunsch
- Performance monitoring: Comprehensive timing, memory, and CPU tracking
- Working directory: `/app`
- Data mount: `./data:/data`

## Performance Monitoring

The Docker image is built with performance logging enabled. Performance metrics are automatically tracked and output to:
- `ancestralclust_performance.csv` in the output directory
- Console summary at program completion

**Performance metrics include:**
- Timing for all major workflow phases
- Memory usage (RSS, VmSize) tracking
- CPU utilization during compute phases
- Thread performance for parallel operations
- Clustering convergence metrics