# Resolve project directory path
SCRIPT_PATH=$( dirname -- "$( readlink -f -- "$0"; )"; )
PROJECT_DIR=$SCRIPT_PATH/..

# Enter project directory
cd $PROJECT_DIR

# Build the container
docker build --tag 'w8ress' -f docker/Dockerfile .