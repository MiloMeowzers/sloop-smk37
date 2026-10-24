# Serves one of the run's installer sites on http://localhost:8766/ for an offline install (no internet needed:
# localhost is a secure context, so Chrome / Edge allow Web MIDI there). Static files only, this machine only.
#   powershell -ExecutionPolicy Bypass -File serve-installer.ps1 pro      (or elite, mke)
# Then open http://localhost:8766/webapp/installer/ in Chrome or Edge. Ctrl+C stops it.
param([ValidateSet("pro", "elite", "mke")][string]$Model = "pro", [int]$Port = 8766)
$root = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "installer-$Model"))
if (-not (Test-Path (Join-Path $root "webapp\installer\index.html"))) { throw "no installer site in $root" }
$types = @{ ".html" = "text/html; charset=utf-8"; ".js" = "text/javascript"; ".css" = "text/css"; ".png" = "image/png";
            ".svg" = "image/svg+xml"; ".ttf" = "font/ttf"; ".json" = "application/json";
            ".webmanifest" = "application/manifest+json"; ".fwsc" = "application/octet-stream"; ".txt" = "text/plain" }
$l = [System.Net.HttpListener]::new()
$l.Prefixes.Add("http://localhost:$Port/")
$l.Start()
Write-Host "SLOOP SMK-37 ($Model) installer: http://localhost:$Port/webapp/installer/   (Ctrl+C to stop)"
try {
    while ($l.IsListening) {
        $c = $l.GetContext()
        $p = [Uri]::UnescapeDataString($c.Request.Url.AbsolutePath.TrimStart("/"))
        if ($p -eq "" -or $p.EndsWith("/")) { $p += "index.html" }
        $f = [System.IO.Path]::GetFullPath((Join-Path $root $p))
        if ($f.StartsWith($root + [System.IO.Path]::DirectorySeparatorChar) -and (Test-Path $f -PathType Leaf)) {
            $b = [System.IO.File]::ReadAllBytes($f)
            $t = $types[[System.IO.Path]::GetExtension($f)]
            if ($t) { $c.Response.ContentType = $t }
            $c.Response.OutputStream.Write($b, 0, $b.Length)
        } else { $c.Response.StatusCode = 404 }
        $c.Response.Close()
    }
} finally { $l.Stop() }
