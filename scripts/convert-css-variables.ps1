#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Batch converts CSS files to use CSS variables
.DESCRIPTION
    Replaces hardcoded color values with CSS variable references across all CSS files.
    This script performs systematic color replacements following the theme system.
#>

$cssDir = "$PSScriptRoot\..\src\resources\ui\css"
$files = @(
    "controls.css",
    "amp.css",
    "effects.css",
    "signal-path.css",
    "modals.css",
    "fx-library.css"
)

# Define color mappings: hardcoded color -> CSS variable
$colorMappings = @{
    # Primary colors
    '#e07848' = 'var(--color-primary)'
    '#c86030' = 'var(--color-primary-dark)'
    '#c86038' = 'var(--color-primary-dark)'
    '#e88858' = 'var(--color-primary-light)'
    '#d87048' = 'var(--color-primary)'
    
    # Text colors
    '#3a3a40' = 'var(--text-primary)'
    '#2a2a30' = 'var(--text-dark-primary)'
    '#4a4a50' = 'var(--text-primary)'
    '#5a5a68' = 'var(--text-secondary)'
    '#6a6a78' = 'var(--text-tertiary)'
    '#8a8a98' = 'var(--text-secondary)'
    '#a0a0b0' = 'var(--text-tertiary)'
    
    # Background gradients -> solid backgrounds
    'linear-gradient\(180deg, #d0d4dc 0%, #b8bcc8 100%\)' = 'var(--bg-secondary)'
    'linear-gradient\(180deg, #e0e4ec 0%, #c8ccd8 100%\)' = 'var(--bg-tertiary)'
    'linear-gradient\(180deg, #f8f8fc, #e8eaf0\)' = 'var(--bg-primary)'
    'linear-gradient\(145deg, #f0f2f8, #d8dce4\)' = 'var(--control-knob-bg)'
    
    # Borders
    '#9098a8' = 'var(--border-darker)'
    '#a0a8b8' = 'var(--border-dark)'
    '#b0b4c0' = 'var(--border-medium)'
    '#b8bcc8' = 'var(--border-light)'
    '#c8ccd8' = 'var(--border-lighter)'
    
    # Backgrounds
    '#ffffff' = 'var(--bg-primary)'
    '#f8f8fc' = 'var(--bg-primary)'
    '#f0f0f4' = 'var(--bg-secondary)'
    '#d8d8e0' = 'var(--bg-tertiary)'
    '#d0d4dc' = 'var(--bg-secondary)'
    '#c8ccd4' = 'var(--bg-tertiary)'
    '#b8bcc4' = 'var(--bg-tertiary)'
    
    # Overlays (rgba)
    'rgba\(255, 255, 255, 0\.3\)' = 'var(--overlay-light)'
    'rgba\(255, 255, 255, 0\.4\)' = 'var(--overlay-medium)'
    'rgba\(255, 255, 255, 0\.6\)' = 'var(--overlay-medium)'
    'rgba\(255, 255, 255, 0\.8\)' = 'var(--overlay-bright)'
    'rgba\(0, ?0, ?0, ?0\.1\)' = 'var(--overlay-dark)'
    'rgba\(0, ?0, ?0, ?0\.2\)' = 'var(--overlay-darker)'
    'rgba\(0, ?0, ?0, ?0\.3\)' = 'var(--overlay-darker)'
    
    # Special colors
    '#fff8f0' = 'var(--color-preset-bg)'
    '#e0d0c0' = 'var(--color-preset-border)'
    '#c8a090' = 'var(--color-preset-favorite)'
    '#5a5048' = 'var(--color-preset-text)'
    '#dc3545' = 'var(--color-error)'
}

Write-Host "CSS Variable Conversion Script" -ForegroundColor Cyan
Write-Host "===============================" -ForegroundColor Cyan
Write-Host ""

foreach ($file in $files) {
    $filePath = Join-Path $cssDir $file
    
    if (!(Test-Path $filePath)) {
        Write-Host "Warning: Skipping $file (not found)" -ForegroundColor Yellow
        continue
    }
    
    Write-Host "Processing: $file" -ForegroundColor Green
    
    $content = Get-Content $filePath -Raw
    $originalContent = $content
    $replacements = 0
    
    foreach ($pattern in $colorMappings.Keys) {
        $variable = $colorMappings[$pattern]
        $matches = [regex]::Matches($content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        
        if ($matches.Count -gt 0) {
            $content = [regex]::Replace($content, $pattern, $variable, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
            $replacements += $matches.Count
            Write-Host "  Replaced $($matches.Count) instances of pattern: $pattern" -ForegroundColor DarkGray
        }
    }
    
    if ($content -ne $originalContent) {
        Set-Content -Path $filePath -Value $content -NoNewline
        Write-Host "  Saved $replacements replacements" -ForegroundColor Cyan
    } else {
        Write-Host "  No changes needed" -ForegroundColor DarkGray
    }
    
    Write-Host ""
}

Write-Host "Conversion complete!" -ForegroundColor Green
Write-Host ""
Write-Host "Next steps:" -ForegroundColor Yellow
Write-Host "1. Review the converted files for any missed colors" -ForegroundColor White
Write-Host "2. Test each theme: default, light, dark, and classic" -ForegroundColor White
Write-Host "3. Build TypeScript and test the application" -ForegroundColor White
