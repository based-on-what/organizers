import os
import win32com.client as win32
import pywintypes
from pathlib import Path

def convert_doc_to_docx(input_folder=None):
    """
    Convert all .doc files in the input folder to .docx format.
    
    Args:
        input_folder (str, optional): Path to input folder. Defaults to current working directory.
    """
    # Use current directory if no input folder specified
    if input_folder is None:
        input_folder = os.getcwd()
    
    input_path = Path(input_folder)
    output_path = input_path / "output"
    
    # Create output folder if it doesn't exist
    output_path.mkdir(exist_ok=True)
    
    # Find all .doc files (excluding .docx)
    doc_files = [f for f in input_path.iterdir() 
                 if f.suffix.lower() == '.doc' and not f.name.lower().endswith('.docx')]
    
    if not doc_files:
        print("No .doc files found in the input folder.")
        return
    
    print(f"Found {len(doc_files)} .doc file(s) to convert.")
    
    # Initialize Microsoft Word
    word = None
    converted_count = 0
    
    try:
        word = win32.Dispatch("Word.Application")
        word.Visible = False
        
        # Process each .doc file
        for doc_file in doc_files:
            output_file = output_path / f"{doc_file.stem}.docx"
            
            try:
                # Open and save as .docx
                doc = word.Documents.Open(str(doc_file))
                doc.SaveAs(str(output_file), FileFormat=16)  # 16 = wdFormatXMLDocument
                doc.Close()
                
                print(f"✓ Successfully converted: {doc_file.name}")
                converted_count += 1
                
            except pywintypes.com_error as e:
                print(f"✗ COM error processing {doc_file.name}: {str(e)}")
                continue
            except Exception as e:
                print(f"✗ Unexpected error with {doc_file.name}: {str(e)}")
                continue
    
    except Exception as e:
        print(f"Failed to initialize Microsoft Word: {str(e)}")
        return
    
    finally:
        # Ensure Word is closed even if an error occurs
        if word:
            try:
                word.Quit()
            except:
                pass
    
    print(f"\nConversion complete!")
    print(f"Successfully converted: {converted_count}/{len(doc_files)} files")
    print(f"Converted files saved in: {output_path}")

if __name__ == "__main__":
    convert_doc_to_docx()
