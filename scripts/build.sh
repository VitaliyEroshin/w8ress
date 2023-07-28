# Resolve project directory path
SCRIPT_PATH=$( dirname -- "$( readlink -f -- "$0"; )"; )
PROJECT_DIR=$SCRIPT_PATH/..

# Enter project directory
cd $PROJECT_DIR

# Build the project with CMake
cmake -B build
cmake --build build

