#pragma once
#include <QByteArray>

// Static utility class: converts raw ROS1 payload bytes into a DDS-CDR
// wrapped QByteArray that can be stored directly in the messages.data column.
class CDRConverter {
public:
    // Wraps [data, size) in a DDS_CDR encapsulation frame.
    // The resulting bytes are ROS2-style: 4-byte encapsulation header
    // followed by the original payload.
    static void packRawBytesToCDR(const char* data, int size, QByteArray& out_cdr);
};
