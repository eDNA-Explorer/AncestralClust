# Use Ubuntu 20.04 as base image (matching CI requirements from README)
FROM ubuntu:20.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# Install required packages including OpenMP support
RUN apt-get update && apt-get install -y \
    build-essential \
    gcc \
    libc6-dev \
    zlib1g-dev \
    make \
    libomp-dev \
    && rm -rf /var/lib/apt/lists/*

# Create app directory
WORKDIR /app

# Copy source code
COPY . .

# Create a fallback mm_malloc.h for non-Intel systems
RUN echo '#ifndef _MM_MALLOC_H_INCLUDED' > /usr/include/mm_malloc.h && \
    echo '#define _MM_MALLOC_H_INCLUDED' >> /usr/include/mm_malloc.h && \
    echo '#include <malloc.h>' >> /usr/include/mm_malloc.h && \
    echo '#define _mm_malloc(size, alignment) malloc(size)' >> /usr/include/mm_malloc.h && \
    echo '#define _mm_free(ptr) free(ptr)' >> /usr/include/mm_malloc.h && \
    echo '#endif' >> /usr/include/mm_malloc.h

# Build the application with performance logging support
RUN make clean && \
    make performance

# Create a volume mount point for input/output files
VOLUME ["/data"]

# Set the executable as entrypoint
ENTRYPOINT ["./ancestralclust"]

# Default help command
CMD ["--help"]