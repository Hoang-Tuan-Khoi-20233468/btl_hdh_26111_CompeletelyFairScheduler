#!/bin/bash
CPUS=$1
CMD=$2
TIMEOUT_SEC=$3
EXPECTED=$4

echo "=================================================="
echo "Running test: make CPUS=$CPUS qemu"
echo "Command: $CMD"
echo "Timeout: $TIMEOUT_SEC seconds"
echo "Expected Output: $EXPECTED"
echo "=================================================="

# Sử dụng lệnh expect để tự động hóa tương tác với QEMU
expect -c "
set timeout $TIMEOUT_SEC
spawn make CPUS=$CPUS qemu

# Đợi cho xv6 boot xong và hiện dấu nhắc lệnh
expect \"init: starting sh\"
send \"$CMD\r\"

if {\"$EXPECTED\" != \"\"} {
    expect {
        \"$EXPECTED\" {
            send \"\001x\" ;# Gửi tín hiệu Ctrl+A rồi X để thoát QEMU
            exit 0
        }
        \"panic\" {
            send \"\001x\"
            exit 1
        }
        timeout {
            send \"\001x\"
            exit 1
        }
    }
} else {
    # Nếu không có EXPECTED (áp dụng cho lệnh grind), chỉ cần đợi xem có panic hay bị timeout không
    expect {
        \"panic\" {
            send \"\001x\"
            exit 1
        }
        timeout {
            send \"\001x\"
            exit 0
        }
    }
}
"
RESULT=$?

if [ $RESULT -eq 0 ]; then
    echo ">>> TEST PASSED!"
    exit 0
else
    echo ">>> TEST FAILED OR TIMED OUT!"
    exit 1
fi