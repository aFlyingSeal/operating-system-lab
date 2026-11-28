# Lab 2 — LaTeX source

Mã nguồn báo cáo Lab 2.

## Cấu trúc

- `report.tex` — file chính.
- `reportDoCS2025.cls` — class của bộ môn.
- `biblio.tex` — tài liệu tham khảo.
- `sections/` — nội dung báo cáo:
  - `credits.tex` — bảng phân chia công việc.
  - `part1.tex` — mục tiêu và phạm vi.
  - `part2.tex` — cơ sở lý thuyết: bảng trang Sv39, `walk()`, `PTE_LEAF`, bố cục bộ nhớ.
  - `part3.tex` — mở rộng `sysinfo` với `freemem`.
  - `part4.tex` — `vmprint()` và giải thích từng trang lá.
  - `part5.tex` — `pgaccess()` và chương trình kiểm thử.
  - `conclusion.tex` — kết luận.
- `latexmkrc` — cấu hình build bằng XeLaTeX.
- `.gitignore` — bỏ qua file build của LaTeX.

## Build

Chạy `latexmk report.tex` trong thư mục này.

Chỉ định rõ `report.tex` vì trong thư mục còn `biblio.tex` là mảnh nội dung, không phải tài liệu độc lập. File kết quả là `report.pdf`; bản nộp được copy sang `report/lab2/report.pdf`.

Yêu cầu: XeLaTeX (có `fontspec`/`polyglossia` cho tiếng Việt), `tcolorbox`, `minted` và `pygments` (`python3 -c "import pygments"`).
