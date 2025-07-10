import os
from pathlib import Path
from PyPDF2 import PdfReader
import ebooklib
from ebooklib import epub
from mobi import Mobi
from docx import Document

def count_epub_pages(file_path):
    """
    Counts approximate pages in an EPUB file
    """
    try:
        book = epub.read_epub(file_path)
        # Count HTML documents as pages
        pages = len(list(book.get_items_of_type(ebooklib.ITEM_DOCUMENT)))
        return pages
    except Exception as e:
        raise Exception(f"Error processing EPUB: {str(e)}")

def count_mobi_pages(file_path):
    """
    Counts approximate pages in a MOBI file
    """
    try:
        book = Mobi(file_path)
        book.parse()
        # Estimate pages based on character count (approx. 2000 per page)
        chars_per_page = 2000
        content_length = len(book.get_book_content())
        pages = max(1, content_length // chars_per_page)
        return pages
    except Exception as e:
        raise Exception(f"Error processing MOBI: {str(e)}")

def count_word_pages(file_path):
    """
    Counts pages in a DOC/DOCX file using page breaks
    """
    try:
        doc = Document(file_path)
        # Count page breaks and add 1 for the first page
        page_breaks = 0
        for paragraph in doc.paragraphs:
            if paragraph.runs:
                page_breaks += len(paragraph.runs[-1].element.xpath("./w:br[@w:type='page']"))
        return page_breaks + 1
    except Exception as e:
        raise Exception(f"Error processing Word file: {str(e)}")

def count_file_pages(file_path):
    """
    Counts pages in a file based on its extension
    """
    file_path = Path(file_path)
    extension = file_path.suffix.lower()
    
    if extension == '.pdf':
        reader = PdfReader(file_path, strict=False)
        return len(reader.pages)
    elif extension == '.epub':
        return count_epub_pages(file_path)
    elif extension == '.mobi':
        return count_mobi_pages(file_path)
    elif extension == '.docx':
        return count_word_pages(file_path)
    else:
        raise Exception(f"Unsupported file format: {extension}")

def count_pages_in_directory(directory):
    """
    Counts pages in PDF, EPUB, MOBI, and DOC/DOCX files in the specified directory.
    """
    directory = Path(directory)
    files_with_pages = []
    error_files = []

    # List all files with supported extensions
    supported_extensions = ('.pdf', '.epub', '.mobi', '.doc', '.docx')
    files = [f for f in directory.iterdir() 
             if f.is_file() and f.suffix.lower() in supported_extensions]
    
    total_files = len(files)
    print(f"\nStarting processing of {total_files} files...\n")

    for index, file_path in enumerate(files, 1):
        print(f"Processing file {index}/{total_files}: {file_path.name}...", end=" ")
        
        try:
            # Check if file is empty
            if file_path.stat().st_size == 0:
                print("ERROR: Empty file")
                error_files.append((file_path.name, "Empty file"))
                continue

            # Count pages based on file type
            page_count = count_file_pages(file_path)
            files_with_pages.append((file_path.name, page_count))
            print(f"COMPLETED ({page_count} pages)")

        except Exception as e:
            print(f"ERROR: {str(e)}")
            error_files.append((file_path.name, str(e)))

    print("\nFile processing completed!")

    # Print error summary
    if error_files:
        print("\nSummary of files with errors:")
        for filename, error in error_files:
            print(f"Error in {filename}:")
            print(f"  - Error type: {error}")
            print(f"  - Full path: {directory / filename}")
            print()

    return files_with_pages

def sort_and_save(files_with_pages, output_file):
    """
    Sorts files by page count, prints the result, and saves it to a file.
    """
    # Sort files by page count
    sorted_files = sorted(files_with_pages, key=lambda x: x[1])

    # Display in console
    print("\nFiles sorted by page count:")
    total_pages = 0
    for filename, pages in sorted_files:
        print(f"{filename}: {pages} pages")
        total_pages += pages
    
    print(f"\nTotal files: {len(sorted_files)}")
    print(f"Total pages: {total_pages}")

    # Save to output file using UTF-8
    output_path = Path(output_file)
    with output_path.open('w', encoding='utf-8') as f:
        f.write("Files sorted by page count:\n")
        f.write("=" * 40 + "\n\n")
        for filename, pages in sorted_files:
            f.write(f"{filename}: {pages} pages\n")
        f.write(f"\nSummary:\n")
        f.write(f"Total files: {len(sorted_files)}\n")
        f.write(f"Total pages: {total_pages}\n")

def main():
    """
    Main function to execute the page counting process
    """
    # Current directory
    current_directory = Path.cwd()
    
    print(f"Scanning directory: {current_directory}")

    # Count pages in files
    files_with_pages = count_pages_in_directory(current_directory)

    if not files_with_pages:
        print("No supported files found in the directory.")
        return

    # Sort and save results
    output_file = "page_count_results.txt"
    sort_and_save(files_with_pages, output_file)

    print(f"\nResults have been saved to '{output_file}'.")

if __name__ == "__main__":
    main()
