FROM gcr.io/oss-fuzz-base/base-builder

# Install dependencies
RUN apt-get update && apt-get install -y cmake

# Copy source
COPY . $SRC/semihost
WORKDIR $SRC/semihost

# Build script is provided by build.sh
COPY .clusterfuzzlite/build.sh $SRC/
