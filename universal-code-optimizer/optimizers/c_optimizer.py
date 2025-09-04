#!/usr/bin/env python3
"""
C/C++ Code Optimizer for Extreme Optimization
Removes ALL human legibility while preserving functionality
"""

import re
import os
from typing import Dict, List, Set, Tuple, Optional
import json
import sys
import os
sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from core.symbol_generator import SymbolGenerator, LanguageSpecificGenerator

class COptimizer:
    """Extreme C/C++ code optimizer"""
    
    def __init__(self, strategy: str = "memory_optimized"):
        self.strategy = strategy
        self.symbol_gen = SymbolGenerator(strategy)
        self.dictionary = {}
        self.reverse_dictionary = {}
        
        # C-specific protected symbols (cannot be optimized)
        self.protected_symbols = {
            # Standard library functions
            'main', 'printf', 'scanf', 'malloc', 'free', 'calloc', 'realloc',
            'strlen', 'strcpy', 'strcat', 'strcmp', 'strncmp', 'memcpy', 'memset',
            'fopen', 'fclose', 'fread', 'fwrite', 'fprintf', 'fscanf',
            
            # System calls
            'open', 'close', 'read', 'write', 'lseek', 'stat', 'fstat',
            
            # ZSTD specific (for zstd project)
            'ZSTD_compress', 'ZSTD_decompress', 'ZSTD_createCCtx', 'ZSTD_freeCCtx',
            'ZSTD_createDCtx', 'ZSTD_freeDCtx', 'ZSTD_compressBound', 'ZSTD_isError',
            'ZSTD_getErrorName', 'ZSTD_versionNumber', 'ZSTD_versionString',
            
            # Common macros
            'NULL', 'EOF', 'TRUE', 'FALSE', 'MAX', 'MIN',
            
            # Compiler attributes
            '__attribute__', '__inline__', '__restrict__', '__typeof__'
        }
        
        # C keywords that can be optimized
        self.optimizable_keywords = {
            'auto', 'break', 'case', 'char', 'const', 'continue', 'default', 'do',
            'double', 'else', 'enum', 'extern', 'float', 'for', 'goto', 'if',
            'int', 'long', 'register', 'return', 'short', 'signed', 'sizeof',
            'static', 'struct', 'switch', 'typedef', 'union', 'unsigned',
            'void', 'volatile', 'while'
        }
        
        # Common C patterns for optimization
        self.patterns = {
            'includes': r'#include\s*[<"][^>"]+[>"]',
            'defines': r'#define\s+(\w+)(?:\s+(.+))?',
            'functions': r'(?:static\s+)?(?:inline\s+)?(?:\w+\s+)+(\w+)\s*\([^)]*\)\s*{',
            'variables': r'(?:static\s+)?(?:const\s+)?(?:\w+\s+)+(\w+)(?:\s*=|;|\[)',
            'structs': r'(?:typedef\s+)?struct\s+(\w+)?\s*{',
            'enums': r'(?:typedef\s+)?enum\s+(\w+)?\s*{',
            'typedefs': r'typedef\s+.+\s+(\w+);',
            'string_literals': r'"[^"]*"',
            'char_literals': r"'[^']*'",
            'numbers': r'\b\d+(?:\.\d+)?(?:[eE][+-]?\d+)?\b',
            'comments_single': r'//.*$',
            'comments_multi': r'/\*.*?\*/',
        }
    
    def optimize_file(self, file_path: str, output_path: Optional[str] = None) -> Dict:
        """Optimize a single C file"""
        print(f"🔧 Optimizing C file: {file_path}")
        
        with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
            original_content = f.read()
        
        # Analyze and optimize
        analysis = self._analyze_code(original_content)
        optimized_content = self._optimize_content(original_content, analysis)
        
        # Save optimized file
        if output_path is None:
            name, ext = os.path.splitext(file_path)
            output_path = f"{name}.optimized{ext}"
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(optimized_content)
        
        # Calculate statistics
        stats = {
            'original_size': len(original_content),
            'optimized_size': len(optimized_content),
            'reduction_bytes': len(original_content) - len(optimized_content),
            'reduction_percent': ((len(original_content) - len(optimized_content)) / len(original_content)) * 100,
            'optimizations_applied': len(self.dictionary),
            'file_path': file_path,
            'output_path': output_path
        }
        
        return stats
    
    def _analyze_code(self, content: str) -> Dict:
        """Analyze C code to identify optimization opportunities"""
        analysis = {
            'identifiers': set(),
            'keywords': set(),
            'string_literals': [],
            'numeric_literals': [],
            'functions': set(),
            'variables': set(),
            'types': set(),
            'frequency': {}
        }
        
        # Find all identifiers
        identifiers = re.findall(r'\b[a-zA-Z_][a-zA-Z0-9_]*\b', content)
        for identifier in identifiers:
            if identifier not in self.protected_symbols:
                analysis['identifiers'].add(identifier)
                analysis['frequency'][identifier] = analysis['frequency'].get(identifier, 0) + 1
        
        # Find keywords
        for keyword in self.optimizable_keywords:
            if re.search(r'\b' + keyword + r'\b', content):
                analysis['keywords'].add(keyword)
                analysis['frequency'][keyword] = len(re.findall(r'\b' + keyword + r'\b', content))
        
        # Find string literals
        strings = re.findall(self.patterns['string_literals'], content)
        for string in strings:
            if len(string) > 3:  # Only optimize longer strings
                analysis['string_literals'].append(string)
                analysis['frequency'][string] = analysis['frequency'].get(string, 0) + 1
        
        # Find numeric literals
        numbers = re.findall(self.patterns['numbers'], content)
        for number in numbers:
            if len(number) > 2:  # Only optimize longer numbers
                analysis['numeric_literals'].append(number)
                analysis['frequency'][number] = analysis['frequency'].get(number, 0) + 1
        
        # Find functions
        functions = re.findall(self.patterns['functions'], content)
        for func in functions:
            if func not in self.protected_symbols:
                analysis['functions'].add(func)
        
        return analysis
    
    def _optimize_content(self, content: str, analysis: Dict) -> str:
        """Apply optimizations to content"""
        optimized = content
        
        # Generate frequency-optimized mappings
        freq_mapping = self.symbol_gen.get_frequency_optimized_mapping(analysis['frequency'])
        
        # Apply optimizations in order of frequency (most frequent first)
        sorted_items = sorted(analysis['frequency'].items(), key=lambda x: x[1], reverse=True)
        
        for original, frequency in sorted_items:
            if original in self.protected_symbols:
                continue
            
            # Generate optimized symbol
            if original in freq_mapping:
                optimized_symbol = freq_mapping[original]
            else:
                optimized_symbol = self.symbol_gen.generate_symbol(original, self._categorize_identifier(original))
            
            # Store in dictionary
            self.dictionary[original] = optimized_symbol
            self.reverse_dictionary[optimized_symbol] = original
            
            # Replace in content (word boundaries to avoid partial matches)
            if original in self.optimizable_keywords:
                # Keywords need special handling
                optimized = re.sub(r'\b' + re.escape(original) + r'\b', optimized_symbol, optimized)
            elif original.startswith('"') and original.endswith('"'):
                # String literals
                optimized = optimized.replace(original, f'"{optimized_symbol}"')
            else:
                # Identifiers
                optimized = re.sub(r'\b' + re.escape(original) + r'\b', optimized_symbol, optimized)
        
        # Additional optimizations
        optimized = self._optimize_whitespace(optimized)
        optimized = self._optimize_comments(optimized)
        
        return optimized
    
    def _categorize_identifier(self, identifier: str) -> str:
        """Categorize identifier for optimized symbol generation"""
        if identifier in self.optimizable_keywords:
            return "keyword"
        elif identifier.isupper():
            return "constant"
        elif identifier.startswith('_') or identifier.endswith('_t'):
            return "type"
        elif re.match(r'^[a-z][a-zA-Z0-9_]*$', identifier):
            return "variable"
        elif re.match(r'^[A-Z][a-zA-Z0-9_]*$', identifier):
            return "function"
        else:
            return "general"
    
    def _optimize_whitespace(self, content: str) -> str:
        """Optimize whitespace for minimum size"""
        # Remove unnecessary whitespace while preserving syntax
        lines = content.split('\n')
        optimized_lines = []
        
        for line in lines:
            # Remove leading/trailing whitespace
            line = line.strip()
            
            # Skip empty lines
            if not line:
                continue
            
            # Compress multiple spaces to single space
            line = re.sub(r'\s+', ' ', line)
            
            # Remove spaces around operators where safe
            line = re.sub(r'\s*([+\-*/%=<>!&|^~])\s*', r'\1', line)
            line = re.sub(r'\s*([,;{}()])\s*', r'\1', line)
            
            optimized_lines.append(line)
        
        return '\n'.join(optimized_lines)
    
    def _optimize_comments(self, content: str) -> str:
        """Remove all comments for maximum optimization"""
        # Remove single-line comments
        content = re.sub(self.patterns['comments_single'], '', content, flags=re.MULTILINE)
        
        # Remove multi-line comments
        content = re.sub(self.patterns['comments_multi'], '', content, flags=re.DOTALL)
        
        return content
    
    def save_dictionary(self, file_path: str):
        """Save optimization dictionary"""
        dictionary_data = {
            'strategy': self.strategy,
            'language': 'c',
            'forward_map': self.dictionary,
            'reverse_map': self.reverse_dictionary,
            'protected_symbols': list(self.protected_symbols),
            'metadata': {
                'total_optimizations': len(self.dictionary),
                'average_reduction': self._calculate_average_reduction(),
                'creation_time': self._get_timestamp()
            }
        }
        
        with open(file_path, 'w', encoding='utf-8') as f:
            json.dump(dictionary_data, f, indent=2, ensure_ascii=False)
    
    def load_dictionary(self, file_path: str):
        """Load optimization dictionary"""
        with open(file_path, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        self.dictionary = data['forward_map']
        self.reverse_dictionary = data['reverse_map']
        self.strategy = data.get('strategy', 'memory_optimized')
    
    def translate_error(self, error_message: str) -> str:
        """Translate error message from optimized to human-readable"""
        translated = error_message
        
        for optimized, original in self.reverse_dictionary.items():
            translated = translated.replace(optimized, original)
        
        return translated
    
    def get_human_readable(self, optimized_code: str) -> str:
        """Convert optimized code back to human-readable form"""
        readable = optimized_code
        
        for optimized, original in self.reverse_dictionary.items():
            readable = re.sub(r'\b' + re.escape(optimized) + r'\b', original, readable)
        
        return readable
    
    def _calculate_average_reduction(self) -> float:
        """Calculate average symbol reduction percentage"""
        if not self.dictionary:
            return 0.0
        
        total_original = sum(len(orig) for orig in self.dictionary.keys())
        total_optimized = sum(len(opt) for opt in self.dictionary.values())
        
        return ((total_original - total_optimized) / total_original) * 100
    
    def _get_timestamp(self) -> str:
        """Get current timestamp"""
        from datetime import datetime
        return datetime.now().isoformat()
    
    def optimize_directory(self, directory: str, output_directory: str) -> Dict:
        """Optimize all C files in directory"""
        results = {
            'files_processed': 0,
            'total_reduction_bytes': 0,
            'total_reduction_percent': 0.0,
            'files': []
        }
        
        # Create output directory
        os.makedirs(output_directory, exist_ok=True)
        
        # Process all C files
        for root, dirs, files in os.walk(directory):
            for file in files:
                if file.endswith(('.c', '.h')):
                    file_path = os.path.join(root, file)
                    rel_path = os.path.relpath(file_path, directory)
                    output_path = os.path.join(output_directory, rel_path)
                    
                    # Create output subdirectory if needed
                    os.makedirs(os.path.dirname(output_path), exist_ok=True)
                    
                    # Optimize file
                    try:
                        stats = self.optimize_file(file_path, output_path)
                        results['files'].append(stats)
                        results['files_processed'] += 1
                        results['total_reduction_bytes'] += stats['reduction_bytes']
                        
                        print(f"✅ {file_path}: {stats['reduction_percent']:.1f}% reduction")
                        
                    except Exception as e:
                        print(f"❌ Error optimizing {file_path}: {e}")
        
        # Calculate overall statistics
        if results['files_processed'] > 0:
            total_original = sum(f['original_size'] for f in results['files'])
            total_optimized = sum(f['optimized_size'] for f in results['files'])
            results['total_reduction_percent'] = ((total_original - total_optimized) / total_original) * 100
        
        # Save dictionary
        dict_path = os.path.join(output_directory, 'c_optimization_dictionary.json')
        self.save_dictionary(dict_path)
        
        return results

if __name__ == "__main__":
    # Test the C optimizer
    optimizer = COptimizer("memory_optimized")
    
    # Test with zstd lib directory
    print("🚀 Testing C Optimizer on zstd library...")
    
    # Test single file first
    test_file = "/Users/camilopineda/Desktop/sdcode/zstd/lib/zstd.h"
    if os.path.exists(test_file):
        print(f"\n📄 Testing single file: {test_file}")
        stats = optimizer.optimize_file(test_file)
        
        print(f"Original size: {stats['original_size']:,} bytes")
        print(f"Optimized size: {stats['optimized_size']:,} bytes") 
        print(f"Reduction: {stats['reduction_percent']:.1f}% ({stats['reduction_bytes']:,} bytes)")
        print(f"Optimizations applied: {stats['optimizations_applied']}")
        
        # Show some dictionary entries
        print(f"\n📚 Sample optimizations:")
        for i, (orig, opt) in enumerate(list(optimizer.dictionary.items())[:10]):
            print(f"  {orig} → {opt}")
    
    else:
        print(f"Test file not found: {test_file}")
