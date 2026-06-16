# Hướng dẫn chi tiết: Thiết lập bản đồ và Va chạm bằng Tiled (.tmx)

Tài liệu này hướng dẫn chi tiết từng bước cách thiết lập bản đồ, cấu trúc layer, và cấu hình va chạm/bậc thang (dốc) bằng phần mềm **Tiled** để game nạp và xử lý chính xác cả về mặt đồ họa (Client) và vật lý (Server).

---

## BƯỚC 1: Thiết lập cấu hình bản đồ trong Tiled

Khi tạo bản đồ mới hoặc chỉnh sửa bản đồ hiện tại (`untitled.tmx`), bạn cần cấu hình các thuộc tính của Map như sau:

1. **Orientation (Hướng bản đồ)**: Chọn **Isometric** (Góc nhìn nghiêng).
2. **Tile Size (Kích thước ô gạch)**:
   - **Tile Width**: `32px` (hoặc kích thước gốc tileset của bạn).
   - **Tile Height**: `16px` (tỉ lệ 2:1 cho góc nghiêng Isometric).
3. **Map Properties (Thuộc tính bản đồ)**:
   - Mở menu **Map** -> **Map Properties** (Bản đồ -> Thuộc tính bản đồ).
   - Nhìn sang bảng thuộc tính bên trái:
     - **Infinite**: Bạn có thể bật **Infinite** (Bản đồ vô hạn) hoặc đặt kích thước cố định (Fixed Size) đều được. Hệ thống đã được nâng cấp hỗ trợ hoàn hảo cả hai dạng.
     - **Tile Layer Format**: Đảm bảo chọn **Base64 (zlib compressed)**. Định dạng này nén dữ liệu giúp dung lượng file cực kỳ nhẹ đối với map lớn.

---

## BƯỚC 2: Cấu trúc các Layer (Lớp bản đồ)

Bản đồ của bạn được chia thành hai nhóm layer chính: **Layer Địa hình** và **Layer Va chạm (`Collision`)**.

### 1. Các Layer Địa hình (Vẽ hình ảnh địa hình)
Game sẽ đọc các layer địa hình này từ dưới lên trên theo thứ tự xuất hiện trong Tiled và tự động gán cao độ Z cho từng layer:
* **Layer địa hình 1 (dưới cùng)**: Cao độ **Z = 0px** (Z-level 0).
* **Layer địa hình 2 (ở giữa)**: Cao độ **Z = 44px** (Z-level 1).
* **Layer địa hình 3 (trên cùng)**: Cao độ **Z = 88px** (Z-level 2).

> [!NOTE]
> Bạn có thể đặt tên các layer địa hình này tùy ý (ví dụ: `Tile Layer 1`, `Tile Layer 2`, `Tile Layer 3` hoặc `Dat Nền`, `Tren Doi`...). Hệ thống sẽ tự động đếm thứ tự xuất hiện của chúng để gán độ cao Z tương ứng.

### 2. Layer Va chạm `Collision` (Quy định thuộc tính vật lý)
* Bạn **bắt buộc** phải tạo thêm một Tile Layer mới trong Tiled và đặt tên chính xác là: **`Collision`** (chữ C viết hoa hoặc viết thường đều được).
* Layer này dùng để vẽ các thuộc tính vật lý như: chỗ nào đi được, chỗ nào bị chặn, chỗ nào là dốc (bậc thang).

---

## BƯỚC 3: Tạo Tileset va chạm và vẽ lên Layer `Collision`

Để vẽ thuộc tính va chạm lên lớp `Collision`, bạn cần chuẩn bị một hình ảnh tileset va chạm nhỏ trong game:

### 1. Chuẩn bị hình ảnh va chạm (`collision_tiles.png`)
Tạo một file ảnh kích thước **128x32 px** gồm 4 ô vuông màu (kích thước mỗi ô 32x32 px) theo thứ tự từ trái qua phải:
1. **Ô 1 (Index 0)**: Màu xanh lá cây $\rightarrow$ **Walkable** (Đi lại được).
2. **Ô 2 (Index 1)**: Màu đỏ $\rightarrow$ **Blocked** (Vật cản cứng/Tường/Vách đá).
3. **Ô 3 (Index 2)**: Màu vàng hoặc mũi tên NE $\rightarrow$ **Ramp_SW_NE** (Dốc đi lên hướng Đông Bắc).
4. **Ô 4 (Index 3)**: Màu xanh dương hoặc mũi tên NW $\rightarrow$ **Ramp_SE_NW** (Dốc đi lên hướng Tây Bắc).

### 2. Thêm Tileset vào Tiled
- Trong Tiled, nhấp nút **New Tileset** (Thêm Tileset mới).
- Chọn file ảnh `collision_tiles.png` ở trên.
- Đặt kích thước tile là **32x32 px**.
- Đặt tên Tileset này tùy ý (ví dụ: `collision`).

### 3. Vẽ thuộc tính va chạm
- Chọn Layer **`Collision`** trong danh sách Layer của Tiled.
- Chọn các ô màu từ Tileset va chạm bạn vừa thêm để vẽ đè lên các vị trí tương ứng trên bản đồ:
  - Vẽ **ô màu đỏ (Index 1)** lên tất cả các vách đá, tường nhà, hồ nước... để chặn nhân vật không cho đi qua.
  - Vẽ **ô màu vàng (Index 2)** hoặc **xanh dương (Index 3)** lên vị trí của các bậc thang/cầu dốc (xem chi tiết quy tắc ở Bước 4).
  - *Mẹo:* Các khu vực đất phẳng thông thường bạn **không cần vẽ gì** (để trống), game sẽ tự động coi các ô trống đó là **Walkable** (Index 0).

---

## BƯỚC 4: Quy tắc Level Design cho Bậc thang (Dốc/Ramp)

Đây là quy tắc cực kỳ quan trọng để nhân vật có thể leo lên/xuống giữa các tầng độ cao Z khác nhau mà không bị kẹt.

### 1. Quy tắc vẽ gạch dốc địa hình
* **Gạch dốc địa hình phải được vẽ ở lớp độ cao đích đến (Layer cao hơn).**
* *Ví dụ:* Bạn muốn làm một cầu thang nối từ `Tile Layer 1` (Z = 0) lên `Tile Layer 2` (Z = 44):
  - Ô gạch dốc đó phải được vẽ trên **`Tile Layer 2`**.
  - Ô phẳng bên cạnh dốc ở tầng dưới phải nằm trên `Tile Layer 1`.
  - Lúc này, Server sẽ tính: Ô dốc có cao độ 44px, ô bên cạnh có cao độ 0px. Khi nhân vật bước vào dốc, độ cao Z của họ sẽ được nội suy mượt mà tăng dần từ 0px lên 44px!

### 2. Quy tắc vẽ va chạm cho dốc trên Layer `Collision`
* Bạn chọn Layer **`Collision`** và vẽ đè tile dốc tương ứng lên đúng vị trí ô gạch dốc địa hình đó:
  - Nếu bậc thang hướng từ dưới-trái (Tây Nam) lên trên-phải (Đông Bắc): Vẽ **ô màu vàng (Index 2)**.
  - Nếu bậc thang hướng từ dưới-phải (Đông Nam) lên trên-trái (Tây Bắc): Vẽ **ô màu xanh dương (Index 3)**.

---

## Ví dụ thực tế minh họa một cầu thang nối Z=0 lên Z=44

Giả sử bạn có bậc thang nối từ trái (thấp, Z=0) sang phải (cao, Z=44) tại tọa độ dòng `Y = 5`, từ cột `X = 10` đến `X = 12`:

1. **Tại `Tile Layer 1` (Z=0)**:
   - Vẽ nền cỏ phẳng từ cột `X = 0` đến cột `X = 10`.
2. **Tại `Tile Layer 2` (Z=44)**:
   - Vẽ ô gạch dốc địa hình tại cột `X = 11`.
   - Vẽ nền đất phẳng cao từ cột `X = 12` đến cột `X = 30`.
3. **Tại Layer `Collision`**:
   - Tại cột `X = 11` (ô dốc): Vẽ **ô màu vàng (Index 2)** để khai báo đây là dốc SW_NE.
   - Tại các ô vách núi dựng đứng xung quanh: Vẽ **ô màu đỏ (Index 1)** để chặn.
   - Khi chạy game, nhân vật đi từ cột 10 (Z=0) bước vào cột 11 (dốc) sẽ tự động được nâng độ cao lên 44px để bước tiếp sang cột 12 (Z=44) phẳng mượt mà!
