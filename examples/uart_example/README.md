# UART usage

[UART usage documentation on the project Wiki](https://github.com/microsoft/Azure-Sphere-DevX/wiki/Working-with-UART)

This example uses a UART loopback connection to exercise the dx_uart implementation.  To use the example insert a jumper wire between the Avnet Starter Kit Click Socket #1 TX and RX signals.

Use the Starter Kit buttons to send unique messages out the UART.  The UART handler will catch the UART data and output the message to debug.