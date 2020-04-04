Report
===

## problems faced and solved in realizing Pipe
1. Remember to `exit` after the son process executed. Otherwise no result.<br/>
2. When trying to create a pipe and use `dup2`, it's necessary to save `STDIN_FILENO` previously and reload it after executed the pipe instruction. Otherwise the left part of the pipe instruction may be automatically executed for several times, and end with segmentation fault. (Don't know why)
