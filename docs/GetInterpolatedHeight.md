# Tài Liệu Kỹ Thuật: Nội Suy Độ Cao Địa Hình ($z$)

Tài liệu này giải thích chi tiết cơ sở toán học và công thức nội suy được sử dụng trong hàm `GetInterpolatedHeight(x, y)` thuộc lớp `MapData` để xác định cao độ $z$ của một thực thể tại vị trí toạ độ thế giới liên tục $(x, y)$.

---

## 1. Các Ký Hiệu và Tham Số Toán Học

Để chuẩn hóa công thức, chúng ta định nghĩa các ký hiệu toán học sau:

*   $(x, y) \in \mathbb{R}^2$: Tọa độ Descartes liên tục của thực thể trong không gian thế giới (World Coordinates).
*   $S \in \mathbb{R}^+$: Kích thước của một ô gạch sau khi đã tỷ lệ hóa (`scaledSize`).
*   $H_{\text{block}} \in \mathbb{R}^+$: Chiều cao chuẩn của một bậc địa hình (`blockHeight`), biểu diễn bằng số pixel.
*   $(t_x, t_y) \in \mathbb{Z}^2$: Chỉ số hàng và cột của ô gạch chứa điểm $(x, y)$ (Tile Coordinates).
*   $T(t_x, t_y)$: Loại ô gạch (Tile Type) tại vị trí $(t_x, t_y)$, ví dụ: ô gạch phẳng thông thường, dốc nâng dần từ Tây Nam lên Đông Bắc, hay dốc nâng dần từ Đông Nam lên Tây Bắc.
*   $h_{\text{base}}(t_x, t_y) \in \mathbb{R}$: Cao độ cơ sở của ô gạch $(t_x, t_y)$ (chiều cao tại mặt đất của ô gạch đó).
*   $z \in \mathbb{R}$: Cao độ nội suy đầu ra tại điểm $(x, y)$.

---

## 2. Xác Định Tọa Độ Ô Gạch

Trước tiên, hệ thống chuyển đổi tọa độ thế giới liên tục $(x, y)$ sang tọa độ ô gạch rời rạc $(t_x, t_y)$ bằng phép toán phần nguyên (hàm sàn - floor function):

$$t_x = \left\lfloor \frac{x}{S} \right\rfloor$$
$$t_y = \left\lfloor \frac{y}{S} \right\rfloor$$

Nếu ô gạch nằm ngoài biên bản đồ (tức là $t_x < 0$, $t_y < 0$, $t_x \ge W$ hoặc $t_y \ge H$, với $W, H$ lần lượt là chiều rộng và chiều cao của bản đồ tính theo số ô gạch):

$$z = 0$$

Ngược lại, ta truy cập loại ô gạch $T(t_x, t_y)$ và độ cao cơ sở $h_{\text{base}}(t_x, t_y)$.

---

## 3. Công Thức Nội Suy Cao Độ ($z$) Theo Loại Ô Gạch

Tùy thuộc vào giá trị của loại ô gạch $T(t_x, t_y)$, độ cao $z$ được xác định theo các trường hợp sau:

### Trường hợp 1: Ô gạch phẳng thông thường
Nếu ô gạch không phải là dốc (tức $T$ là mặt phẳng ngang hoặc vật cản thông thường), cao độ là hằng số trên toàn bộ diện tích ô gạch:

$$z = h_{\text{base}}(t_x, t_y)$$

---

### Trường hợp 2: Dốc từ Tây Nam lên Đông Bắc ($T = \text{Ramp\_SW\_NE}$)
Dốc này có độ cao tăng dần khi di chuyển từ Tây sang Đông (tương ứng với chiều tăng của trục tọa độ Descartes $x$ trong ô gạch).

1.  **Khoảng cách cục bộ** của $x$ tính từ biên trái của ô gạch:
    $$\Delta x = x - t_x \cdot S$$

2.  **Tỉ lệ nội suy** $r$ (tỉ lệ khoảng cách so với kích thước ô gạch):
    $$r = \frac{\Delta x}{S}$$

3.  **Hàm kẹp (Clamp)** giới hạn tỉ lệ trong đoạn $[0, 1]$ nhằm tránh các sai số do tính toán dấu phẩy động:
    $$r_{\text{clamped}} = \max\left(0, \min\left(1, r\right)\right)$$

4.  **Độ cao nội suy** $z$:
    $$z = h_{\text{base}}(t_x, t_y) + r_{\text{clamped}} \cdot H_{\text{block}}$$

---

### Trường hợp 3: Dốc từ Đông Nam lên Tây Bắc ($T = \text{Ramp\_SE\_NW}$)
Dốc này có độ cao tăng dần khi đi từ Nam lên Bắc (tương ứng với chiều giảm của trục tọa độ Descartes $y$ trong ô gạch).

1.  **Khoảng cách cục bộ** của $y$ tính từ biên trên của ô gạch:
    $$\Delta y = y - t_y \cdot S$$

2.  **Tỉ lệ nội suy** $r$. Do cao độ tăng khi $y$ giảm (đi lên phía Bắc), tỉ lệ nội suy tỉ lệ nghịch với khoảng cách cục bộ $\Delta y$:
    $$r = \frac{S - \Delta y}{S} = 1 - \frac{\Delta y}{S}$$

3.  **Hàm kẹp (Clamp)** giới hạn tỉ lệ trong đoạn $[0, 1]$:
    $$r_{\text{clamped}} = \max\left(0, \min\left(1, r\right)\right)$$

4.  **Độ cao nội suy** $z$:
    $$z = h_{\text{base}}(t_x, t_y) + r_{\text{clamped}} \cdot H_{\text{block}}$$

---

## 4. Tổng Kết Thuật Toán Dưới Dạng Hàm Toán Học

Hàm nội suy cao độ có thể được biểu diễn tổng quát bằng toán học như sau:

$$z(x, y) = \begin{cases} 
0, & \text{nếu } (t_x, t_y) \text{ ngoài biên bản đồ} \\
h_{\text{base}}(t_x, t_y) + \text{clamp}\left(0, 1, \frac{x - t_x \cdot S}{S}\right) \cdot H_{\text{block}}, & \text{nếu } T(t_x, t_y) = \text{Ramp\_SW\_NE} \\
h_{\text{base}}(t_x, t_y) + \text{clamp}\left(0, 1, 1 - \frac{y - t_y \cdot S}{S}\right) \cdot H_{\text{block}}, & \text{nếu } T(t_x, t_y) = \text{Ramp\_SE\_NW} \\
h_{\text{base}}(t_x, t_y), & \text{cho các trường hợp còn lại}
\end{cases}$$

Trong đó hàm kẹp được định nghĩa là:
$$\text{clamp}(a, b, v) = \max(a, \min(b, v))$$
