#!/usr/bin/env python3
"""
Enhanced Comic and Manga Page Counter

This script analyzes comic books and graphic novels in various formats (CBZ, CBR, EPUB, PDF)
and counts pages for organizing collections. Features improved error handling and shared utilities.
"""

import sys
import zipfile
from pathlib import Path
from typing import Dict

try:
    from pypdf import PdfReader  # Modern replacement for PyPDF2
except ImportError:
    try:
        from PyPDF2 import PdfReader  # Fallback to old library
        print("Warning: Using deprecated PyPDF2. Please install pypdf instead.")
    except ImportError:
        print("Error: Neither pypdf nor PyPDF2 is available. Install with: pip install pypdf")
        sys.exit(1)

try:
    from ebooklib import epub
except ImportError:
    print("Error: ebooklib is required. Install with: pip install ebooklib")
    sys.exit(1)

try:
    from rarfile import RarFile
except ImportError:
    print("Error: rarfile is required. Install with: pip install rarfile")
    sys.exit(1)

from shared_utils import (
    setup_logging, safe_file_operation, save_results_to_file, 
    find_files_by_extensions, ProgressReporter
)

# Supported image extensions for comic files
IMAGE_EXTENSIONS = {'.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp'}

def count_pages_cbz(cbz_file: Path) -> int:
    """Count the number of pages (images) in a .cbz file."""
    try:
        with zipfile.ZipFile(cbz_file, 'r') as archive:
            return sum(1 for file_name in archive.namelist() 
                      if Path(file_name).suffix.lower() in IMAGE_EXTENSIONS)
    except Exception as e:
        raise Exception(f"Error reading CBZ file: {e}")


def count_pages_cbr(cbr_file: Path) -> int:
    """Count the number of pages (images) in a .cbr file."""
    try:
        with RarFile(cbr_file, 'r') as archive:
            return sum(1 for file_name in archive.namelist() 
                      if Path(file_name).suffix.lower() in IMAGE_EXTENSIONS)
    except Exception as e:
        raise Exception(f"Error reading CBR file: {e}")


def count_pages_epub(epub_file: Path) -> int:
    """Count the number of pages (HTML chapters) in an .epub file."""
    try:
        book = epub.read_epub(str(epub_file))
        return sum(1 for item in book.get_items() if isinstance(item, epub.EpubHtml))
    except Exception as e:
        raise Exception(f"Error reading EPUB file: {e}")


def count_pages_pdf(pdf_file: Path) -> int:
    """Count the number of pages in a .pdf file."""
    try:
        with open(pdf_file, 'rb') as file:
            reader = PdfReader(file)
            return len(reader.pages)
    except Exception as e:
        raise Exception(f"Error reading PDF file: {e}")

def get_page_counter(file_extension: str):
    """Return the appropriate page counter function for the given file extension."""
    counters = {
        '.cbz': count_pages_cbz,
        '.cbr': count_pages_cbr,
        '.epub': count_pages_epub,
        '.pdf': count_pages_pdf
    }
    return counters.get(file_extension.lower())

def count_pages_in_file(file_path: Path) -> int:
    """Count pages in a supported file format."""
    counter = get_page_counter(file_path.suffix)
    if counter:
        return counter(file_path)
    return 0

def count_pages_in_directory(main_directory: Path) -> Dict[str, int]:
    """
    Count pages in supported comic/book files within a directory and its subdirectories.
    
    Args:
        main_directory: Path to the main directory to analyze
        
    Returns:
        Dictionary mapping file/directory names to page counts
    """
    results = {}
    supported_extensions = {'.cbz', '.cbr', '.epub', '.pdf'}
    
    if not main_directory.exists():
        logging.error(f"Directory {main_directory} does not exist")
        return results
    
    logging.info(f"Analyzing directory: {main_directory}")
    logging.info("-" * 50)
    
    # Process subdirectories and individual files
    for item in main_directory.iterdir():
        if item.is_dir():
            # Process subdirectory
            total_pages = 0
            file_count = 0
            
            # Find all supported files in subdirectory recursively
            files = find_files_by_extensions(item, supported_extensions, recursive=True)
            
            if files:
                progress = ProgressReporter(len(files), f"Processing {item.name}")
                
                for file_path in files:
                    try:
                        if safe_file_operation(file_path, "page counting"):
                            pages = count_pages_in_file(file_path)
                            total_pages += pages
                            file_count += 1
                        else:
                            logging.warning(f"Skipping inaccessible file: {file_path}")
                    except Exception as e:
                        logging.error(f"Error processing {file_path.name}: {e}")
                    
                    progress.update()
                
                progress.finish()
            
            results[item.name] = total_pages
            logging.info(f"📁 {item.name}: {file_count} files, {total_pages} pages")
            
        elif item.suffix.lower() in supported_extensions:
            # Process individual file
            try:
                if safe_file_operation(item, "page counting"):
                    pages = count_pages_in_file(item)
                    results[item.name] = pages
                    logging.info(f"📄 {item.name}: {pages} pages")
                else:
                    logging.warning(f"Skipping inaccessible file: {item}")
                    results[item.name] = 0
            except Exception as e:
                logging.error(f"Error processing {item.name}: {e}")
                results[item.name] = 0
    
    return results

def display_results(results: Dict[str, int]) -> None:
    """Display results sorted by page count and save to file."""
    if not results:
        logging.info("No supported files found.")
        return
    
    # Sort results by page count (ascending)
    sorted_results = sorted(results.items(), key=lambda x: x[1])
    
    logging.info("\n" + "=" * 50)
    logging.info("FINAL RESULTS (sorted by page count)")
    logging.info("=" * 50)
    
    total_pages = 0
    for name, pages in sorted_results:
        logging.info(f"{name}: {pages} pages")
        total_pages += pages
    
    logging.info("-" * 50)
    logging.info(f"Total items: {len(results)}")
    logging.info(f"Total pages: {total_pages}")
    
    # Save results to file using shared utility
    results_text = [f"{name}: {pages} pages" for name, pages in sorted_results]
    results_text.extend([
        "",
        f"Total items: {len(results)}",
        f"Total pages: {total_pages}"
    ])
    
    output_file = Path("page_count_results.txt")
    if save_results_to_file(results_text, output_file, "COMIC/MANGA PAGE COUNT RESULTS"):
        logging.info(f"💾 Results saved to: {output_file}")


def main(directory: Path = None) -> None:
    """
    Main function to analyze a directory for supported comic/book files.
    
    Args:
        directory: Directory to analyze. Defaults to script directory.
    """
    # Set up logging
    logger = setup_logging("INFO")
    
    if directory is None:
        # Get the directory where the script is located
        directory = Path(__file__).parent.absolute()
    
    logger.info(f"Starting comic/manga analysis in: {directory}")
    
    results = count_pages_in_directory(directory)
    display_results(results)


if __name__ == "__main__":
    main()
