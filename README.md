# inspace-contracts

## To build and deploy the Filepace contract

* `git clone https://github.com/inSpaceTechnologies/inspace-contracts/`
* `cd inspace-contracts/filespace`
* `eosiocpp -o filespace.wast filespace.cpp`
* `eosiocpp -g filespace.abi filespace.cpp`
* `cleos set contract filespace ../filespace`
