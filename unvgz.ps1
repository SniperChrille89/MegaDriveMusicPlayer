param(
    [Parameter(Mandatory=$true)][string]$InputFile,
    [Parameter(Mandatory=$true)][string]$OutputFile
)
$ErrorActionPreference = 'Stop'
$source = $null
$gzip = $null
$destination = $null
try {
    $source = [System.IO.File]::OpenRead($InputFile)
    $gzip = New-Object System.IO.Compression.GZipStream($source, [System.IO.Compression.CompressionMode]::Decompress)
    $destination = [System.IO.File]::Create($OutputFile)
    $gzip.CopyTo($destination)
    exit 0
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
} finally {
    if ($destination) { $destination.Dispose() }
    if ($gzip) { $gzip.Dispose() }
    if ($source) { $source.Dispose() }
}
