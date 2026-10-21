#!/bin/bash

ScriptDir="$(cd "$(dirname "$0")" && pwd -P)"
cd "$ScriptDir" || exit 1

ProjectOut="$(find ./Release -maxdepth 1 -type f -name "*_Appli.bin" -print -quit)"
if [ -z "$ProjectOut" ]; then
    echo "Error: application binary was not found in $ScriptDir/Release" >&2
    exit 1
fi

arm-none-eabi-objcopy -I binary "$ProjectOut" \
    --change-addresses 0x70100400 -O ihex \
    "$ScriptDir"/../../Binary/appli.hex
