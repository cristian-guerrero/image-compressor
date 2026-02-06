$bytes = [System.IO.File]::ReadAllBytes("resources/NotoSansJP-Bold.ttf")
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("unsigned char font_data[] = {")
for ($i = 0; $i -lt $bytes.Length; $i++) {
    [void]$sb.Append("0x{0:X2}" -f $bytes[$i])
    if ($i -lt $bytes.Length - 1) {
        [void]$sb.Append(", ")
        if (($i + 1) % 12 -eq 0) {
            [void]$sb.AppendLine()
        }
    }
}
[void]$sb.AppendLine("};")
[void]$sb.AppendLine("unsigned int font_data_size = $($bytes.Length);")
[System.IO.File]::WriteAllText("src/font_data.h", $sb.ToString())
