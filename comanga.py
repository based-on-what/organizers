import os
import zipfile
from pathlib import Path
from typing import Dict

from ebooklib import epub
from PyPDF2 import PdfReader
from rarfile import RarFile

# Supported image extensions for comic files
IMAGE_EXTENSIONS = {'.jpg', '.jpeg', '.png', '.gif', '.bmp', '.webp'}

def count_pages_cbz(cbz_file: Path) -> int:
    """Count the number of pages (images) in a .cbz file."""
    try:
        with zipfile.ZipFile(cbz_file, 'r') as archive:
            return sum(1 for file_name in archive.namelist() 
                      if Path(file_name).suffix.lower() in IMAGE_EXTENSIONS)
    except Exception as e:
        print(f"Error reading {cbz_file.name}: {e}")
        return 0

def count_pages_cbr(cbr_file: Path) -> int:
    """Count the number of pages (images) in a .cbr file."""
    try:
        with RarFile(cbr_file, 'r') as archive:
            return sum(1 for file_name in archive.namelist() 
                      if Path(file_name).suffix.lower() in IMAGE_EXTENSIONS)
    except Exception as e:
        print(f"Error reading {cbr_file.name}: {e}")
        return 0

def count_pages_epub(epub_file: Path) -> int:
    """Count the number of pages (HTML chapters) in an .epub file."""
    try:
        book = epub.read_epub(str(epub_file))
        return sum(1 for item in book.get_items() if isinstance(item, epub.EpubHtml))
    except Exception as e:
        print(f"Error reading {epub_file.name}: {e}")
        return 0

def count_pages_pdf(pdf_file: Path) -> int:
    """Count the number of pages in a .pdf file."""
    try:
        reader = PdfReader(str(pdf_file))
        return len(reader.pages)
    except Exception as e:
        print(f"Error reading {pdf_file.name}: {e}")
        return 0

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
    Count pages in .cbz, .cbr, .epub and .pdf files within a directory and its subdirectories.
    
    Args:
        main_directory (Path): Path to the main directory to analyze
        
    Returns:
        Dict[str, int]: Dictionary mapping file/directory names to page counts
    """
    results = {}
    supported_extensions = {'.cbz', '.cbr', '.epub', '.pdf'}
    
    if not main_directory.exists():
        print(f"Error: Directory {main_directory} does not exist")
        return results
    
    print(f"Analyzing directory: {main_directory}")
    print("-" * 50)
    
    # Iterate through items in the main directory
    for item in main_directory.iterdir():
        if item.is_dir():
            # If it's a subdirectory, count pages in supported files within it
            total_pages = 0
            file_count = 0
            
            for file_path in item.rglob('*'):
                if file_path.is_file() and file_path.suffix.lower() in supported_extensions:
                    pages = count_pages_in_file(file_path)
                    total_pages += pages
                    file_count += 1
            
            results[item.name] = total_pages
            print(f"📁 Subdirectory: {item.name} | Files: {file_count} | Pages: {total_pages}")
            
        elif item.suffix.lower() in supported_extensions:
            # If it's a supported file, count it individually
            pages = count_pages_in_file(item)
            results[item.name] = pages
            print(f"📄 File: {item.name} | Pages: {pages}")
    
    return results

def save_results_to_file(results: Dict[str, int], output_file: Path = None) -> None:
    """Save results to a text file."""
    if output_file is None:
        output_file = Path("page_count_results.txt")
    
    try:
        with open(output_file, "w", encoding="utf-8") as txt_file:
            txt_file.write("PAGE COUNT RESULTS\n")
            txt_file.write("=" * 50 + "\n\n")
            
            for name, pages in results:
                txt_file.write(f"{name}: {pages} pages\n")
        
        print(f"\n💾 Results saved to: {output_file}")
    except Exception as e:
        print(f"Error saving results to file: {e}")

def display_results(results: Dict[str, int]) -> None:
    """Display results sorted by page count."""
    if not results:
        print("No supported files found.")
        return
    
    # Sort results by page count (ascending)
    sorted_results = sorted(results.items(), key=lambda x: x[1])
    
    print("\n" + "=" * 50)
    print("FINAL RESULTS (sorted by page count)")
    print("=" * 50)
    
    total_pages = 0
    for name, pages in sorted_results:
        print(f"{name}: {pages} pages")
        total_pages += pages
    
    print("-" * 50)
    print(f"Total items: {len(results)}")
    print(f"Total pages: {total_pages}")
    
    # Save results to file
    save_results_to_file(sorted_results)

def main(directory: Path = None) -> None:
    """
    Main function to analyze a directory for supported book/comic files.
    
    Args:
        directory (Path, optional): Directory to analyze. Defaults to script directory.
    """
    if directory is None:
        # Get the directory where the script is located
        script_directory = Path(__file__).parent.absolute()
        directory = script_directory
    
    results = count_pages_in_directory(directory)
    display_results(results)

if __name__ == "__main__":
    main()
