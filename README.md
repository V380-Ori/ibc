# ibc

(i)nhibit (b)attery (c)harging on Apple silicon Mac

**This tool requires superuser privileges to communicate with System Management Controller (AppleSMC).**

## Usage

```bash
sudo ./ibc
```

This tool toggles the battery charging state on your Mac. Running it will:
- **Enable** charging if it's currently disabled
- **Disable** charging if it's currently enabled

The tool will output the SMC key and its new value after it's been written.

## Example Output

```
CH0C -> 00          # Charging enabled
CH0C -> 01          # Charging disabled
```

or

```
CHTE -> 00000000    # Charging enabled  (macOS 15.7+)
CHTE -> 00000001    # Charging disabled (macOS 15.7+)
```

## Building

```bash
make
```

or

```bash
clang -framework IOKit src/ibc.c -o ibc
```

**Use with caution**: This tool directly modifies hardware charging behavior. Improper use could potentially affect battery health or system stability.

