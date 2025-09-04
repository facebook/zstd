#!/usr/bin/env python3
"""
Universal Symbol Generator for Extreme Code Optimization
Generates optimized symbols for maximum memory and speed efficiency
"""

import string
import itertools
from typing import Dict, List, Set, Tuple, Optional
import json
import hashlib

class SymbolGenerator:
    """Generates optimized symbols using various strategies"""
    
    def __init__(self, strategy: str = "memory_optimized"):
        self.strategy = strategy
        self.used_symbols = set()
        self.symbol_map = {}
        self.reverse_map = {}
        
        # Different symbol sets for different strategies
        self.symbol_sets = {
            "memory_optimized": {
                # Ultra-short symbols for maximum memory efficiency
                "single": list("αβγδεζηθικλμνξοπρστυφχψω") + list("ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΤΥΦΧΨΩ"),
                "double": [],  # Will be generated
                "triple": []   # Will be generated
            },
            
            "speed_optimized": {
                # Single-byte symbols for maximum speed
                "single": [chr(i) for i in range(33, 127) if chr(i) not in '"\'\\`'],
                "double": [],
                "triple": []
            },
            
            "hybrid": {
                # Mix of readable and optimized
                "single": list("αβγδεζηθικλμνξοπρστυφχψω") + list(string.ascii_lowercase),
                "double": [],
                "triple": []
            }
        }
        
        self._generate_extended_symbols()
    
    def _generate_extended_symbols(self):
        """Generate double and triple character symbols"""
        base_set = self.symbol_sets[self.strategy]["single"]
        
        # Generate double symbols
        for a, b in itertools.product(base_set[:20], repeat=2):  # Limit to prevent explosion
            self.symbol_sets[self.strategy]["double"].append(a + b)
        
        # Generate triple symbols (limited set)
        for a, b, c in itertools.product(base_set[:10], repeat=3):
            self.symbol_sets[self.strategy]["triple"].append(a + b + c)
    
    def generate_symbol(self, original: str, category: str = "general") -> str:
        """
        Generate optimized symbol for given original string
        
        Args:
            original: Original identifier/keyword
            category: Category of symbol (keyword, function, variable, etc.)
        
        Returns:
            Optimized symbol
        """
        if original in self.symbol_map:
            return self.symbol_map[original]
        
        # Strategy-based symbol generation
        if self.strategy == "memory_optimized":
            symbol = self._generate_memory_optimized(original, category)
        elif self.strategy == "speed_optimized":
            symbol = self._generate_speed_optimized(original, category)
        else:  # hybrid
            symbol = self._generate_hybrid(original, category)
        
        # Ensure uniqueness
        while symbol in self.used_symbols:
            symbol = self._next_symbol(symbol)
        
        # Store mapping
        self.symbol_map[original] = symbol
        self.reverse_map[symbol] = original
        self.used_symbols.add(symbol)
        
        return symbol
    
    def _generate_memory_optimized(self, original: str, category: str) -> str:
        """Generate symbol optimized for minimum memory usage"""
        # Priority: single char > double char > triple char
        
        # Try single characters first
        for symbol in self.symbol_sets[self.strategy]["single"]:
            if symbol not in self.used_symbols:
                return symbol
        
        # Try double characters
        for symbol in self.symbol_sets[self.strategy]["double"]:
            if symbol not in self.used_symbols:
                return symbol
        
        # Fall back to triple characters
        for symbol in self.symbol_sets[self.strategy]["triple"]:
            if symbol not in self.used_symbols:
                return symbol
        
        # Generate hash-based symbol if all else fails
        return self._generate_hash_symbol(original, 2)
    
    def _generate_speed_optimized(self, original: str, category: str) -> str:
        """Generate symbol optimized for maximum processing speed"""
        # Use single-byte ASCII characters for fastest processing
        
        # Category-based prefixing for better cache locality
        prefixes = {
            "keyword": "k",
            "function": "f", 
            "variable": "v",
            "constant": "c",
            "type": "t",
            "general": "g"
        }
        
        prefix = prefixes.get(category, "g")
        
        # Try single characters with prefix
        for i, symbol in enumerate(self.symbol_sets[self.strategy]["single"]):
            candidate = prefix + symbol
            if candidate not in self.used_symbols:
                return candidate
        
        # Fall back to numeric suffix
        counter = 1
        while True:
            candidate = f"{prefix}{counter}"
            if candidate not in self.used_symbols:
                return candidate
            counter += 1
    
    def _generate_hybrid(self, original: str, category: str) -> str:
        """Generate symbol balancing memory and readability"""
        # Use Greek letters for common items, ASCII for others
        
        # High-frequency items get Greek letters
        high_frequency = ["function", "var", "const", "let", "if", "for", "while", "class", "struct"]
        
        if original.lower() in high_frequency:
            return self._generate_memory_optimized(original, category)
        else:
            return self._generate_speed_optimized(original, category)
    
    def _next_symbol(self, current: str) -> str:
        """Generate next available symbol"""
        if len(current) == 1:
            # Try next single character
            chars = self.symbol_sets[self.strategy]["single"]
            try:
                idx = chars.index(current)
                if idx + 1 < len(chars):
                    return chars[idx + 1]
            except ValueError:
                pass
        
        # Add character or increment
        return current + self.symbol_sets[self.strategy]["single"][0]
    
    def _generate_hash_symbol(self, original: str, length: int = 3) -> str:
        """Generate hash-based symbol as fallback"""
        hash_obj = hashlib.md5(original.encode())
        hash_hex = hash_obj.hexdigest()
        
        # Convert to symbol using available character set
        chars = self.symbol_sets[self.strategy]["single"]
        symbol = ""
        
        for i in range(length):
            idx = int(hash_hex[i*2:i*2+2], 16) % len(chars)
            symbol += chars[idx]
        
        return symbol
    
    def get_frequency_optimized_mapping(self, frequency_data: Dict[str, int]) -> Dict[str, str]:
        """
        Generate symbols optimized by frequency of use
        Most frequent items get shortest symbols
        """
        # Sort by frequency (descending)
        sorted_items = sorted(frequency_data.items(), key=lambda x: x[1], reverse=True)
        
        mapping = {}
        
        # Assign shortest symbols to most frequent items
        single_chars = self.symbol_sets[self.strategy]["single"]
        double_chars = self.symbol_sets[self.strategy]["double"]
        
        for i, (original, freq) in enumerate(sorted_items):
            if i < len(single_chars):
                # Most frequent get single characters
                symbol = single_chars[i]
            elif i < len(single_chars) + len(double_chars):
                # Next most frequent get double characters
                symbol = double_chars[i - len(single_chars)]
            else:
                # Rest get generated symbols
                symbol = self._generate_hash_symbol(original)
            
            mapping[original] = symbol
        
        return mapping
    
    def save_mapping(self, filepath: str):
        """Save symbol mapping to file"""
        mapping_data = {
            "strategy": self.strategy,
            "forward_map": self.symbol_map,
            "reverse_map": self.reverse_map,
            "metadata": {
                "total_symbols": len(self.symbol_map),
                "average_reduction": self._calculate_average_reduction(),
                "memory_savings": self._calculate_memory_savings()
            }
        }
        
        with open(filepath, 'w', encoding='utf-8') as f:
            json.dump(mapping_data, f, indent=2, ensure_ascii=False)
    
    def load_mapping(self, filepath: str):
        """Load symbol mapping from file"""
        with open(filepath, 'r', encoding='utf-8') as f:
            data = json.load(f)
        
        self.strategy = data["strategy"]
        self.symbol_map = data["forward_map"]
        self.reverse_map = data["reverse_map"]
        self.used_symbols = set(self.symbol_map.values())
    
    def _calculate_average_reduction(self) -> float:
        """Calculate average symbol length reduction"""
        if not self.symbol_map:
            return 0.0
        
        total_original = sum(len(orig) for orig in self.symbol_map.keys())
        total_optimized = sum(len(opt) for opt in self.symbol_map.values())
        
        return ((total_original - total_optimized) / total_original) * 100
    
    def _calculate_memory_savings(self) -> int:
        """Calculate total memory savings in bytes"""
        if not self.symbol_map:
            return 0
        
        original_bytes = sum(len(orig.encode('utf-8')) for orig in self.symbol_map.keys())
        optimized_bytes = sum(len(opt.encode('utf-8')) for opt in self.symbol_map.values())
        
        return original_bytes - optimized_bytes
    
    def get_stats(self) -> Dict:
        """Get optimization statistics"""
        return {
            "strategy": self.strategy,
            "total_symbols": len(self.symbol_map),
            "average_reduction": self._calculate_average_reduction(),
            "memory_savings": self._calculate_memory_savings(),
            "symbol_distribution": {
                "single_char": sum(1 for s in self.symbol_map.values() if len(s) == 1),
                "double_char": sum(1 for s in self.symbol_map.values() if len(s) == 2),
                "triple_char": sum(1 for s in self.symbol_map.values() if len(s) == 3),
                "longer": sum(1 for s in self.symbol_map.values() if len(s) > 3)
            }
        }

class LanguageSpecificGenerator:
    """Language-specific symbol generation strategies"""
    
    @staticmethod
    def get_language_priorities(language: str) -> Dict[str, int]:
        """Get optimization priorities for specific languages"""
        priorities = {
            'c': {
                'keywords': ['int', 'char', 'void', 'struct', 'if', 'for', 'while', 'return'],
                'functions': ['malloc', 'free', 'printf', 'scanf', 'strlen', 'strcpy'],
                'types': ['size_t', 'uint32_t', 'int64_t']
            },
            'cpp': {
                'keywords': ['class', 'namespace', 'template', 'public', 'private', 'virtual'],
                'functions': ['std::cout', 'std::cin', 'std::vector', 'std::string'],
                'types': ['std::unique_ptr', 'std::shared_ptr']
            },
            'javascript': {
                'keywords': ['function', 'const', 'let', 'var', 'if', 'for', 'while', 'return'],
                'functions': ['console.log', 'setTimeout', 'setInterval', 'addEventListener'],
                'objects': ['document', 'window', 'Array', 'Object']
            },
            'python': {
                'keywords': ['def', 'class', 'import', 'from', 'if', 'for', 'while', 'return'],
                'functions': ['print', 'len', 'range', 'enumerate', 'zip'],
                'types': ['str', 'int', 'float', 'list', 'dict']
            }
        }
        
        return priorities.get(language, {})

if __name__ == "__main__":
    # Test the symbol generator
    generator = SymbolGenerator("memory_optimized")
    
    # Test common programming constructs
    test_items = [
        "function", "variable", "constant", "if", "for", "while", 
        "class", "struct", "namespace", "public", "private",
        "ZSTD_compress", "ZSTD_decompress", "malloc", "free"
    ]
    
    print("🔤 Testing Symbol Generation:")
    print("=" * 50)
    
    for item in test_items:
        symbol = generator.generate_symbol(item)
        reduction = ((len(item) - len(symbol)) / len(item)) * 100
        print(f"{item:15} → {symbol:8} ({reduction:5.1f}% reduction)")
    
    print(f"\n📊 Statistics:")
    stats = generator.get_stats()
    for key, value in stats.items():
        print(f"  {key}: {value}")
