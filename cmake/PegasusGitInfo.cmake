# Use current date as version instead of git describe
string(TIMESTAMP PEGASUS_GIT_REVISION "%Y-%m-%d")
string(TIMESTAMP PEGASUS_GIT_DATE "%Y-%m-%d")

execute_process(COMMAND
    git
    --git-dir "${PROJECT_SOURCE_DIR}/.git"
    --work-tree "${PROJECT_SOURCE_DIR}"
    rev-list
    --count HEAD
    OUTPUT_STRIP_TRAILING_WHITESPACE
    OUTPUT_VARIABLE PEGASUS_GIT_COMMIT_CNT
)
