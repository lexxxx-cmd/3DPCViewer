#include "CDRConverter.h"

#include <fastcdr/Cdr.h>
#include <fastcdr/FastBuffer.h>

#include <vector>

// FastCDR 2.x API:
//   - CdrVersion enum lives outside Cdr (eprosima::fastcdr::CdrVersion)
//   - get_serialized_data_length() replaces getSerializedDataLength()
//   - serialize_array() replaces serializeArray()

void CDRConverter::packRawBytesToCDR(const char* data, int size, QByteArray& out_cdr)
{
    // Allocate a backing buffer large enough for the 4-byte encapsulation
    // header plus the raw payload.
    const int bufSize = size + 100;
    std::vector<char> buf(static_cast<size_t>(bufSize));

    eprosima::fastcdr::FastBuffer fastbuffer(buf.data(), static_cast<size_t>(bufSize));

    // DDS_CDR: ROS2 standard encapsulation (0x00 0x01 for little-endian).
    eprosima::fastcdr::Cdr cdr_ser(
        fastbuffer,
        eprosima::fastcdr::Cdr::DEFAULT_ENDIAN,
        eprosima::fastcdr::CdrVersion::DDS_CDR);

    // Write the 4-byte DDS encapsulation header.
    cdr_ser.serialize_encapsulation();

    // Append the original ROS1 payload bytes verbatim.
    cdr_ser.serialize_array(reinterpret_cast<const uint8_t*>(data),
                            static_cast<size_t>(size));

    const size_t written = cdr_ser.get_serialized_data_length();
    out_cdr = QByteArray(fastbuffer.getBuffer(), static_cast<int>(written));
}
