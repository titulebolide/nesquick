# Python Nes Emulator

## Development caveats
- self.mem read and write must be done with caution; they can sometime cause sides effects on special mapped devices. For example, each read to the PPUDATA register increases PPUADDR. Things like
```python3
self.mem[addr] = val
self.mem[addr] %= 256
```
must be avoided. We are trying here to emulate a single write, and we actually do two writes and one read!
