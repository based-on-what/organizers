#!/usr/bin/env python3
"""
Enhanced DOC to DOCX Converter

This script converts legacy .doc files to modern .docx format with cross-platform
support and improved error handling.
"""

import sys
import platform
from pathlib import Path
from typing import List, Optional

# Cross-platform compatibility for Word automation
PLATFORM_WINDOWS = platform.system() == "Windows"

if PLATFORM_WINDOWS:
    try:
        import win32com.client as win32
        import pywintypes
        WIN32_AVAILABLE = True
    except ImportError:
        WIN32_AVAILABLE = False
        print("Warning: win32com not available. Some features may be limited on Windows.")
else:
    WIN32_AVAILABLE = False

from shared_utils import setup_logging, ProgressReporter

def find_doc_files(input_path: Path) -> List[Path]:
    """
    Find all .doc files (excluding .docx) in the input directory.
    
    Args:
        input_path: Directory to search
        
    Returns:
        List of .doc file paths
    """
    doc_files = []
    for file_path in input_path.iterdir():
        if (file_path.is_file() and 
            file_path.suffix.lower() == '.doc' and 
            not file_path.name.lower().endswith('.docx')):
            doc_files.append(file_path)
    
    return doc_files


def convert_with_libreoffice(input_file: Path, output_dir: Path) -> bool:
    """
    Convert DOC to DOCX using LibreOffice (cross-platform alternative).
    
    Args:
        input_file: Path to the .doc file
        output_dir: Directory to save the converted file
        
    Returns:
        True if successful, False otherwise
    """
    try:
        import subprocess
        
        # Check if LibreOffice is available
        try:
            result = subprocess.run(['libreoffice', '--version'], 
                                  capture_output=True, text=True, timeout=10)
            if result.returncode != 0:
                return False
        except (subprocess.TimeoutExpired, FileNotFoundError):
            return False
        
        # Convert using LibreOffice headless mode
        cmd = [
            'libreoffice',
            '--headless',
            '--convert-to', 'docx',
            '--outdir', str(output_dir),
            str(input_file)
        ]
        
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        
        if result.returncode == 0:
            logging.info(f"✓ LibreOffice conversion successful: {input_file.name}")
            return True
        else:
            logging.error(f"✗ LibreOffice conversion failed: {result.stderr}")
            return False
            
    except Exception as e:
        logging.error(f"✗ Error with LibreOffice conversion: {e}")
        return False


def convert_with_word(input_file: Path, output_dir: Path, word_app) -> bool:
    """
    Convert DOC to DOCX using Microsoft Word COM interface.
    
    Args:
        input_file: Path to the .doc file
        output_dir: Directory to save the converted file
        word_app: Word application COM object
        
    Returns:
        True if successful, False otherwise
    """
    if not WIN32_AVAILABLE:
        return False
        
    try:
        output_file = output_dir / f"{input_file.stem}.docx"
        
        # Open the document
        doc = word_app.Documents.Open(str(input_file))
        
        # Save as DOCX (FileFormat=16 is wdFormatXMLDocument)
        doc.SaveAs(str(output_file), FileFormat=16)
        doc.Close()
        
        logging.info(f"✓ Word conversion successful: {input_file.name}")
        return True
        
    except Exception as e:
        logging.error(f"✗ Word conversion failed for {input_file.name}: {e}")
        return False


def convert_doc_to_docx(input_folder: Optional[Path] = None) -> None:
    """
    Convert all .doc files in the input folder to .docx format.
    
    Args:
        input_folder: Path to input folder. Defaults to current working directory.
    """
    # Set up logging
    logger = setup_logging("INFO")
    
    # Use current directory if no input folder specified
    if input_folder is None:
        input_folder = Path.cwd()
    else:
        input_folder = Path(input_folder)
    
    output_path = input_folder / "output"
    
    # Create output folder if it doesn't exist
    output_path.mkdir(exist_ok=True)
    logging.info(f"Output directory: {output_path}")
    
    # Find all .doc files
    doc_files = find_doc_files(input_folder)
    
    if not doc_files:
        logging.info("No .doc files found in the input folder.")
        return
    
    logging.info(f"Found {len(doc_files)} .doc file(s) to convert.")
    
    converted_count = 0
    word_app = None
    progress = ProgressReporter(len(doc_files), "Converting files")
    
    # Try to initialize Microsoft Word if on Windows
    if PLATFORM_WINDOWS and WIN32_AVAILABLE:
        try:
            word_app = win32.Dispatch("Word.Application")
            word_app.Visible = False
            logging.info("Using Microsoft Word for conversion")
        except Exception as e:
            logging.warning(f"Could not initialize Microsoft Word: {e}")
            logging.info("Will try LibreOffice as fallback")
    
    try:
        # Process each .doc file
        for doc_file in doc_files:
            success = False
            
            # Try Word first if available
            if word_app:
                success = convert_with_word(doc_file, output_path, word_app)
            
            # Fall back to LibreOffice if Word failed or not available
            if not success:
                success = convert_with_libreoffice(doc_file, output_path)
            
            if success:
                converted_count += 1
            else:
                logging.error(f"✗ Failed to convert: {doc_file.name}")
            
            progress.update()
    
    finally:
        # Ensure Word is closed
        if word_app:
            try:
                word_app.Quit()
            except:
                pass
        
        progress.finish()
    
    # Summary
    logging.info(f"\nConversion complete!")
    logging.info(f"Successfully converted: {converted_count}/{len(doc_files)} files")
    logging.info(f"Converted files saved in: {output_path}")
    
    if converted_count < len(doc_files):
        failed_count = len(doc_files) - converted_count
        logging.warning(f"Failed conversions: {failed_count}")
        logging.info("Note: For best results, install either Microsoft Word or LibreOffice")


def main() -> None:
    """Main function to execute DOC to DOCX conversion."""
    convert_doc_to_docx()


if __name__ == "__main__":
    main()
