// SPDX-License-Identifier: MIT OR Apache-2.0
pragma solidity 0.8.24;

// The few Foundry cheat codes that the test and the script use. This
// project does not use forge-std, so it declares them here.

address constant VM_ADDRESS = 0x7109709ECfa91a80626fF3989D68f67F5b1DD12D;
address constant CONSOLE_ADDRESS = 0x000000000000000000636F6e736F6c652e6c6f67;

interface Vm {
    function projectRoot() external view returns (string memory path);

    function readFile(string calldata path) external view returns (string memory data);

    function parseJsonBytes(string calldata json, string calldata key) external pure returns (bytes memory);

    function parseJsonUintArray(string calldata json, string calldata key) external pure returns (uint256[] memory);

    function startBroadcast() external;

    function stopBroadcast() external;
}

library Console {
    /// Write `name value` to the forge output. `ok` is false when no
    /// console listens (a plain EVM): the log is then lost, nothing else.
    function log(string memory name, address value) internal view returns (bool ok) {
        (ok,) = CONSOLE_ADDRESS.staticcall(abi.encodeWithSignature("log(string,address)", name, value));
    }
}
