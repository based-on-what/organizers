"""
Shared utilities for file organizer scripts.

This module contains common functionality used across multiple organizer scripts
to reduce code duplication and improve maintainability.
"""

import logging
import sys
from pathlib import Path
from typing import Dict, List, Optional, Set, Tuple, Union


def setup_logging(log_level: str = "INFO", log_file: Optional[str] = None) -> logging.Logger:
    """
    Set up logging configuration with timestamps and proper formatting.
    
    Args:
        log_level: Logging level (DEBUG, INFO, WARNING, ERROR)
        log_file: Optional log file path
        
    Returns:
        Configured logger instance
    """
    # Create formatters
    formatter = logging.Formatter('%(asctime)s - %(levelname)s - %(message)s')
    
    # Set up root logger
    logger = logging.getLogger()
    logger.setLevel(getattr(logging, log_level.upper()))
    
    # Clear existing handlers
    logger.handlers.clear()
    
    # Console handler
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setFormatter(formatter)
    logger.addHandler(console_handler)
    
    # File handler (optional)
    if log_file:
        file_handler = logging.FileHandler(log_file, encoding='utf-8')
        file_handler.setFormatter(formatter)
        logger.addHandler(file_handler)
    
    return logger


def format_file_size(size_bytes: int) -> str:
    """
    Format file size in bytes to human-readable format.
    
    Args:
        size_bytes: File size in bytes
        
    Returns:
        Human-readable file size string
    """
    for unit in ['B', 'KB', 'MB', 'GB']:
        if size_bytes < 1024.0:
            return f"{size_bytes:.1f} {unit}"
        size_bytes /= 1024.0
    return f"{size_bytes:.1f} TB"


def format_duration(seconds: float) -> str:
    """
    Format duration in seconds to human-readable format (HH:MM:SS).
    
    Args:
        seconds: Duration in seconds
        
    Returns:
        Formatted duration string
    """
    hours = int(seconds // 3600)
    minutes = int((seconds % 3600) // 60)
    secs = int(seconds % 60)
    return f"{hours:02d}:{minutes:02d}:{secs:02d}"


def validate_file_path(file_path: Union[str, Path], min_size: int = 0) -> bool:
    """
    Validate that a file path exists, is a file, and meets minimum size requirements.
    
    Args:
        file_path: Path to validate
        min_size: Minimum file size in bytes (default: 0)
        
    Returns:
        True if file is valid, False otherwise
    """
    try:
        path = Path(file_path)
        return (
            path.exists() 
            and path.is_file() 
            and path.stat().st_size >= min_size
        )
    except (OSError, PermissionError):
        return False


def safe_file_operation(file_path: Path, operation_name: str = "process") -> bool:
    """
    Check if a file can be safely accessed for operations.
    
    Args:
        file_path: Path to check
        operation_name: Description of operation for logging
        
    Returns:
        True if file is accessible, False otherwise
    """
    try:
        if not file_path.exists():
            logging.warning(f"File does not exist: {file_path}")
            return False
            
        if not file_path.is_file():
            logging.warning(f"Path is not a file: {file_path}")
            return False
            
        # Check if file is empty
        if file_path.stat().st_size == 0:
            logging.warning(f"File is empty: {file_path}")
            return False
            
        # Try to open file for reading to check permissions
        with open(file_path, 'rb') as f:
            f.read(1)  # Try to read one byte
            
        return True
        
    except PermissionError:
        logging.error(f"Permission denied accessing {file_path}")
        return False
    except OSError as e:
        logging.error(f"OS error accessing {file_path}: {e}")
        return False
    except Exception as e:
        logging.error(f"Unexpected error checking {file_path} for {operation_name}: {e}")
        return False


def save_results_to_file(
    results: Union[Dict, List], 
    output_file: Path, 
    title: str = "RESULTS",
    encoding: str = "utf-8"
) -> bool:
    """
    Save results to a text file with consistent formatting.
    
    Args:
        results: Dictionary or list of results to save
        output_file: Path to output file
        title: Title for the results file
        encoding: File encoding (default: utf-8)
        
    Returns:
        True if successful, False otherwise
    """
    try:
        with open(output_file, "w", encoding=encoding) as f:
            f.write(f"{title}\n")
            f.write("=" * len(title) + "\n\n")
            
            if isinstance(results, dict):
                for key, value in results.items():
                    f.write(f"{key}: {value}\n")
            elif isinstance(results, list):
                for item in results:
                    if isinstance(item, tuple) and len(item) == 2:
                        f.write(f"{item[0]}: {item[1]}\n")
                    else:
                        f.write(f"{item}\n")
            else:
                f.write(str(results))
                
        logging.info(f"Results saved to: {output_file}")
        return True
        
    except Exception as e:
        logging.error(f"Error saving results to {output_file}: {e}")
        return False


def find_files_by_extensions(
    directory: Path, 
    extensions: Set[str], 
    exclude_dirs: Optional[Set[str]] = None,
    recursive: bool = True
) -> List[Path]:
    """
    Find files with specific extensions in a directory.
    
    Args:
        directory: Directory to search
        extensions: Set of file extensions to include (with dots, e.g., {'.pdf', '.txt'})
        exclude_dirs: Set of directory names to exclude
        recursive: Whether to search recursively
        
    Returns:
        List of matching file paths
    """
    if exclude_dirs is None:
        exclude_dirs = set()
        
    files = []
    
    try:
        if recursive:
            for root, dirs, filenames in directory.walk():
                # Filter out excluded directories
                dirs[:] = [d for d in dirs if d not in exclude_dirs]
                
                for filename in filenames:
                    file_path = root / filename
                    if file_path.suffix.lower() in extensions:
                        files.append(file_path)
        else:
            for file_path in directory.iterdir():
                if file_path.is_file() and file_path.suffix.lower() in extensions:
                    files.append(file_path)
                    
    except Exception as e:
        logging.error(f"Error searching for files in {directory}: {e}")
        
    return files


def print_summary_stats(items: List, title: str = "Summary") -> None:
    """
    Print summary statistics for a list of items.
    
    Args:
        items: List of items to summarize
        title: Title for the summary section
    """
    count = len(items)
    logging.info("=" * 50)
    logging.info(f"{title.upper()}")
    logging.info("=" * 50)
    logging.info(f"Total items processed: {count}")
    
    if count == 0:
        logging.info("No items to summarize")
    else:
        logging.info(f"Processing completed successfully")


class ProgressReporter:
    """Simple progress reporting utility."""
    
    def __init__(self, total: int, description: str = "Processing"):
        self.total = total
        self.current = 0
        self.description = description
        
    def update(self, increment: int = 1) -> None:
        """Update progress by increment."""
        self.current += increment
        percentage = (self.current / self.total) * 100 if self.total > 0 else 0
        logging.info(f"{self.description}: {self.current}/{self.total} ({percentage:.1f}%)")
        
    def finish(self) -> None:
        """Mark progress as complete."""
        logging.info(f"{self.description} completed: {self.current}/{self.total}")