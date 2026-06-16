#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

// Lớp NetworkBuffer đại diện cho một bộ đệm dữ liệu mạng.
// Giúp tích lũy dữ liệu byte thô nhận được từ socket TCP và bóc tách
// chúng thành các gói tin (Packet) hoàn chỉnh, xử lý các hiện tượng cắt gói (phân mảnh) hoặc gộp gói.
class NetworkBuffer {
public:
  // Thêm dữ liệu byte thô mới nhận từ mạng vào cuối bộ đệm
  void Append(const uint8_t *data, size_t size) {
    buffer.insert(buffer.end(), data, data + size);
  }

  // Bóc tách gói tin hoàn chỉnh đầu tiên từ bộ đệm (nếu có)
  // Trả về true nếu lấy ra được một gói tin đầy đủ, ngược lại trả về false.
  // outId: nhận Type ID của gói tin bóc ra được
  // outData: nhận mảng byte chứa toàn bộ dữ liệu gói tin thô (bao gồm cả Header)
  bool HasCompletePacket(uint16_t &outId, std::vector<uint8_t> &outData) {
    // Một gói tin tối thiểu phải có 4 byte (Header gồm: 2 byte ID + 2 byte Length)
    if (buffer.size() < 4)
      return false;

    // Đọc độ dài gói tin từ byte thứ 2 và thứ 3 trong buffer (độ dài toàn gói)
    uint16_t length;
    std::memcpy(&length, &buffer[2], sizeof(uint16_t));

    // Bảo vệ phòng ngừa trường hợp gói tin bị lỗi chiều dài bất thường
    if (length < 4 || length > 2048) {
      // Nếu độ dài nhỏ hơn 4 hoặc lớn hơn 2048 (lỗi mạng hoặc corrupt data):
      // Xóa toàn bộ bộ đệm để khôi phục khỏi trạng thái lỗi và tránh tràn bộ nhớ
      buffer.clear();
      return false;
    }

    // Nếu kích thước dữ liệu hiện có trong bộ đệm nhỏ hơn độ dài của gói tin mong đợi:
    // Gói tin chưa truyền về đủ (bị phân mảnh) -> Chờ thêm dữ liệu ở các lần đọc tiếp theo.
    if (buffer.size() < length)
      return false;

    // Đọc ID gói tin từ 2 byte đầu tiên
    std::memcpy(&outId, &buffer[0], sizeof(uint16_t));
    
    // Gán dữ liệu gói tin hoàn chỉnh vào biến đầu ra outData
    outData.assign(buffer.begin(), buffer.begin() + length);

    // Xóa phần dữ liệu của gói tin này ra khỏi bộ đệm để sẵn sàng xử lý gói tin tiếp theo
    buffer.erase(buffer.begin(), buffer.begin() + length);
    return true;
  }

  // Xóa sạch bộ đệm dữ liệu
  void Clear() { buffer.clear(); }

  // Lấy số lượng byte hiện đang lưu trữ trong bộ đệm
  [[nodiscard]] size_t GetSize() const noexcept { return buffer.size(); }

private:
  std::vector<uint8_t> buffer; // Mảng vector chứa các byte dữ liệu động
};
