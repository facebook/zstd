#!/usr/bin/env python3
"""
Universal Code Optimizer - Showcase Web Application
Interactive demo showcasing extreme code optimization capabilities
"""

from flask import Flask, render_template_string, request, jsonify, send_file
import os
import json
import tempfile
import threading
import time
from universal_optimizer import UniversalOptimizer

app = Flask(__name__)
optimizer = UniversalOptimizer("memory_optimized")

# Store demo results
demo_results = {
    'optimizations': [],
    'stats': {
        'total_files': 0,
        'total_reduction': 0,
        'average_reduction': 0,
        'languages_processed': set()
    }
}

@app.route('/')
def showcase_home():
    """Main showcase page"""
    return render_template_string(SHOWCASE_HTML_TEMPLATE)

@app.route('/api/optimize-demo', methods=['POST'])
def optimize_demo():
    """Optimize code for demo purposes"""
    data = request.get_json()
    
    code = data.get('code', '')
    language = data.get('language', 'c')
    strategy = data.get('strategy', 'memory_optimized')
    
    if not code.strip():
        return jsonify({'error': 'No code provided'}), 400
    
    try:
        # Create temporary file
        with tempfile.NamedTemporaryFile(mode='w', suffix=f'.{language}', delete=False) as f:
            f.write(code)
            temp_file = f.name
        
        # Create output file
        output_file = temp_file + '.optimized'
        
        # Optimize
        result = optimizer.optimize_file(temp_file, output_file)
        
        if 'error' not in result:
            # Read optimized code
            with open(output_file, 'r', encoding='utf-8', errors='ignore') as f:
                optimized_code = f.read()
            
            # Get translation
            translated_back = optimizer.translate_code(optimized_code, language)
            
            # Store results
            demo_result = {
                'original_code': code,
                'optimized_code': optimized_code,
                'translated_code': translated_back,
                'language': language,
                'strategy': strategy,
                'original_size': len(code),
                'optimized_size': len(optimized_code),
                'reduction_percent': ((len(code) - len(optimized_code)) / len(code)) * 100,
                'timestamp': time.time()
            }
            
            demo_results['optimizations'].append(demo_result)
            demo_results['stats']['total_files'] += 1
            demo_results['stats']['languages_processed'].add(language)
            
            # Update average
            total_reduction = sum(opt['reduction_percent'] for opt in demo_results['optimizations'])
            demo_results['stats']['average_reduction'] = total_reduction / len(demo_results['optimizations'])
            
            # Cleanup
            os.unlink(temp_file)
            os.unlink(output_file)
            
            return jsonify({
                'success': True,
                'result': demo_result
            })
        else:
            return jsonify({'error': result['error']}), 400
            
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@app.route('/api/get-sample-code/<language>')
def get_sample_code(language):
    """Get sample code for different languages"""
    samples = {
        'c': '''#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct UserData {
    int userId;
    char userName[64];
    double balance;
    int isActive;
};

int processUserData(struct UserData* userData) {
    if (userData == NULL) {
        printf("Error: userData is NULL\\n");
        return -1;
    }
    
    if (!userData->isActive) {
        printf("Warning: User %s is inactive\\n", userData->userName);
        return 0;
    }
    
    printf("Processing user: %s (ID: %d, Balance: $%.2f)\\n", 
           userData->userName, userData->userId, userData->balance);
    
    // Simulate processing
    for (int i = 0; i < 10; i++) {
        userData->balance += i * 0.1;
    }
    
    printf("Updated balance: $%.2f\\n", userData->balance);
    return 1;
}

int main() {
    struct UserData user = {
        .userId = 12345,
        .userName = "johnsmith",
        .balance = 1000.50,
        .isActive = 1
    };
    
    int result = processUserData(&user);
    printf("Processing result: %d\\n", result);
    
    return 0;
}''',
        
        'javascript': '''function calculateUserScore(userData, gameResults) {
    const baseScore = 100;
    let totalScore = baseScore;
    
    if (!userData || !gameResults) {
        console.error("Missing required data");
        return -1;
    }
    
    console.log(`Processing score for user: ${userData.username}`);
    
    for (const result of gameResults) {
        if (result.victory) {
            totalScore += result.points * 1.5;
            console.log(`Victory bonus: +${result.points * 1.5}`);
        } else {
            totalScore -= result.points * 0.5;
            console.log(`Loss penalty: -${result.points * 0.5}`);
        }
    }
    
    const finalScore = Math.round(totalScore);
    console.log(`Final score: ${finalScore}`);
    
    return finalScore;
}

const userData = {
    username: "player123",
    level: 15,
    experience: 2500
};

const gameResults = [
    { victory: true, points: 50 },
    { victory: false, points: 30 },
    { victory: true, points: 75 }
];

const score = calculateUserScore(userData, gameResults);''',
        
        'python': '''import json
import datetime
from typing import Dict, List, Optional

class UserManager:
    def __init__(self):
        self.users = {}
        self.active_sessions = []
    
    def create_user(self, username: str, email: str, age: int) -> bool:
        if username in self.users:
            print(f"User {username} already exists")
            return False
        
        user_data = {
            "username": username,
            "email": email,
            "age": age,
            "created_at": datetime.datetime.now().isoformat(),
            "is_active": True,
            "login_count": 0
        }
        
        self.users[username] = user_data
        print(f"Created user: {username}")
        return True
    
    def authenticate_user(self, username: str, password: str) -> Optional[Dict]:
        if username not in self.users:
            print(f"User {username} not found")
            return None
        
        user = self.users[username]
        if not user["is_active"]:
            print(f"User {username} is deactivated")
            return None
        
        # Simulate password check
        user["login_count"] += 1
        user["last_login"] = datetime.datetime.now().isoformat()
        
        session_id = f"session_{len(self.active_sessions)}"
        self.active_sessions.append({
            "session_id": session_id,
            "username": username,
            "created_at": datetime.datetime.now().isoformat()
        })
        
        return {"user": user, "session_id": session_id}

# Demo usage
manager = UserManager()
manager.create_user("alice", "alice@example.com", 25)
result = manager.authenticate_user("alice", "password123")'''
    }
    
    return jsonify({
        'language': language,
        'sample_code': samples.get(language, '// No sample available for this language')
    })

@app.route('/api/stats')
def get_stats():
    """Get showcase statistics"""
    stats = demo_results['stats'].copy()
    stats['languages_processed'] = list(stats['languages_processed'])
    stats['recent_optimizations'] = demo_results['optimizations'][-5:]  # Last 5
    
    return jsonify(stats)

@app.route('/api/download-optimized/<int:index>')
def download_optimized(index):
    """Download optimized code"""
    if 0 <= index < len(demo_results['optimizations']):
        result = demo_results['optimizations'][index]
        
        # Create temporary file
        with tempfile.NamedTemporaryFile(mode='w', suffix='.optimized', delete=False) as f:
            f.write(result['optimized_code'])
            temp_file = f.name
        
        return send_file(temp_file, as_attachment=True, 
                        download_name=f"optimized_{result['language']}_{index}.txt")
    
    return jsonify({'error': 'Optimization not found'}), 404

# HTML Template for the showcase
SHOWCASE_HTML_TEMPLATE = '''
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>🚀 Universal Code Optimizer - Live Showcase</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        body {
            font-family: 'Monaco', 'Consolas', 'SF Mono', monospace;
            background: linear-gradient(135deg, #0c0c0c 0%, #1a1a2e 50%, #16213e 100%);
            color: #e0e0e0;
            min-height: 100vh;
            overflow-x: hidden;
        }
        
        .header {
            background: rgba(0,0,0,0.8);
            backdrop-filter: blur(10px);
            padding: 20px;
            text-align: center;
            border-bottom: 2px solid #4fc3f7;
            position: sticky;
            top: 0;
            z-index: 100;
        }
        
        .header h1 {
            font-size: 2.5rem;
            background: linear-gradient(45deg, #4fc3f7, #29b6f6, #81c784);
            -webkit-background-clip: text;
            -webkit-text-fill-color: transparent;
            margin-bottom: 10px;
            animation: glow 2s ease-in-out infinite alternate;
        }
        
        @keyframes glow {
            from { filter: drop-shadow(0 0 5px #4fc3f7); }
            to { filter: drop-shadow(0 0 20px #29b6f6); }
        }
        
        .tagline {
            font-size: 1.2rem;
            color: #81c784;
            margin-bottom: 15px;
        }
        
        .stats-bar {
            display: flex;
            justify-content: center;
            gap: 30px;
            margin-top: 15px;
        }
        
        .stat {
            text-align: center;
        }
        
        .stat-value {
            font-size: 1.5rem;
            font-weight: bold;
            color: #4fc3f7;
        }
        
        .stat-label {
            font-size: 0.9rem;
            color: #888;
        }
        
        .container {
            max-width: 1400px;
            margin: 0 auto;
            padding: 20px;
        }
        
        .demo-section {
            background: rgba(30, 30, 30, 0.9);
            border-radius: 15px;
            padding: 25px;
            margin-bottom: 25px;
            border: 1px solid #333;
            box-shadow: 0 10px 30px rgba(0,0,0,0.5);
        }
        
        .demo-section h2 {
            color: #4fc3f7;
            margin-bottom: 20px;
            font-size: 1.5rem;
        }
        
        .controls {
            display: grid;
            grid-template-columns: 1fr 1fr 1fr auto;
            gap: 15px;
            margin-bottom: 20px;
            align-items: end;
        }
        
        .control-group {
            display: flex;
            flex-direction: column;
        }
        
        .control-group label {
            color: #4fc3f7;
            margin-bottom: 5px;
            font-weight: bold;
        }
        
        select, button {
            padding: 10px;
            border: 1px solid #555;
            border-radius: 5px;
            background: #2d2d2d;
            color: #e0e0e0;
            font-family: inherit;
        }
        
        button {
            background: linear-gradient(45deg, #4fc3f7, #29b6f6);
            color: #000;
            font-weight: bold;
            cursor: pointer;
            transition: all 0.3s ease;
        }
        
        button:hover {
            background: linear-gradient(45deg, #29b6f6, #4fc3f7);
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(79, 195, 247, 0.4);
        }
        
        button:disabled {
            background: #555;
            color: #888;
            cursor: not-allowed;
            transform: none;
            box-shadow: none;
        }
        
        .code-container {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 20px;
            margin-bottom: 20px;
        }
        
        .code-section {
            display: flex;
            flex-direction: column;
        }
        
        .code-header {
            background: #333;
            color: #4fc3f7;
            padding: 10px 15px;
            border-radius: 5px 5px 0 0;
            font-weight: bold;
            display: flex;
            justify-content: space-between;
            align-items: center;
        }
        
        .code-textarea {
            background: #1a1a1a;
            border: 1px solid #333;
            border-top: none;
            border-radius: 0 0 5px 5px;
            color: #e0e0e0;
            font-family: 'Monaco', 'Consolas', monospace;
            font-size: 13px;
            line-height: 1.4;
            padding: 15px;
            resize: vertical;
            min-height: 400px;
        }
        
        .results-section {
            background: rgba(45, 45, 45, 0.9);
            border-radius: 10px;
            padding: 20px;
            margin-top: 20px;
        }
        
        .result-stats {
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
            gap: 15px;
            margin-bottom: 20px;
        }
        
        .result-stat {
            background: #333;
            padding: 15px;
            border-radius: 8px;
            text-align: center;
        }
        
        .result-stat-value {
            font-size: 1.3rem;
            font-weight: bold;
            color: #81c784;
        }
        
        .result-stat-label {
            color: #aaa;
            margin-top: 5px;
        }
        
        .translation-section {
            margin-top: 20px;
            padding: 20px;
            background: rgba(129, 199, 132, 0.1);
            border: 1px solid #81c784;
            border-radius: 10px;
        }
        
        .translation-header {
            color: #81c784;
            font-weight: bold;
            margin-bottom: 10px;
        }
        
        .error {
            background: rgba(244, 67, 54, 0.1);
            border: 1px solid #f44336;
            color: #f44336;
            padding: 15px;
            border-radius: 5px;
            margin: 10px 0;
        }
        
        .success {
            background: rgba(76, 175, 80, 0.1);
            border: 1px solid #4caf50;
            color: #4caf50;
            padding: 15px;
            border-radius: 5px;
            margin: 10px 0;
        }
        
        .loading {
            display: inline-block;
            width: 20px;
            height: 20px;
            border: 2px solid #333;
            border-radius: 50%;
            border-top-color: #4fc3f7;
            animation: spin 1s ease-in-out infinite;
        }
        
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        
        .copy-btn {
            background: #666;
            color: #fff;
            border: none;
            padding: 5px 10px;
            border-radius: 3px;
            cursor: pointer;
            font-size: 12px;
        }
        
        .copy-btn:hover {
            background: #777;
        }
        
        .showcase-grid {
            display: grid;
            grid-template-columns: 2fr 1fr;
            gap: 20px;
        }
        
        .sidebar {
            background: rgba(30, 30, 30, 0.9);
            border-radius: 15px;
            padding: 20px;
            height: fit-content;
            border: 1px solid #333;
        }
        
        .recent-optimizations {
            margin-top: 20px;
        }
        
        .optimization-item {
            background: #333;
            padding: 10px;
            border-radius: 5px;
            margin-bottom: 10px;
            font-size: 12px;
        }
        
        .optimization-lang {
            color: #4fc3f7;
            font-weight: bold;
        }
        
        .optimization-reduction {
            color: #81c784;
            float: right;
        }
        
        @media (max-width: 768px) {
            .showcase-grid {
                grid-template-columns: 1fr;
            }
            
            .code-container {
                grid-template-columns: 1fr;
            }
            
            .controls {
                grid-template-columns: 1fr;
            }
        }
    </style>
</head>
<body>
    <div class="header">
        <h1>🚀 Universal Code Optimizer</h1>
        <div class="tagline">EXTREME OPTIMIZATION • Remove ALL Human Legibility • Real-time Translation</div>
        <div class="stats-bar">
            <div class="stat">
                <div class="stat-value" id="totalFiles">0</div>
                <div class="stat-label">Files Processed</div>
            </div>
            <div class="stat">
                <div class="stat-value" id="avgReduction">0%</div>
                <div class="stat-label">Avg Reduction</div>
            </div>
            <div class="stat">
                <div class="stat-value" id="languagesCount">0</div>
                <div class="stat-label">Languages</div>
            </div>
        </div>
    </div>

    <div class="container">
        <div class="showcase-grid">
            <div class="main-content">
                <div class="demo-section">
                    <h2>🔥 Live Code Optimization</h2>
                    
                    <div class="controls">
                        <div class="control-group">
                            <label>Programming Language:</label>
                            <select id="languageSelect">
                                <option value="c">C</option>
                                <option value="javascript">JavaScript</option>
                                <option value="python">Python</option>
                            </select>
                        </div>
                        
                        <div class="control-group">
                            <label>Optimization Strategy:</label>
                            <select id="strategySelect">
                                <option value="memory_optimized">Memory Optimized</option>
                                <option value="speed_optimized">Speed Optimized</option>
                                <option value="hybrid">Hybrid</option>
                            </select>
                        </div>
                        
                        <div class="control-group">
                            <label>Sample Code:</label>
                            <button type="button" id="loadSampleBtn">Load Sample</button>
                        </div>
                        
                        <div class="control-group">
                            <label>&nbsp;</label>
                            <button type="button" id="optimizeBtn">🚀 OPTIMIZE</button>
                        </div>
                    </div>
                    
                    <div class="code-container">
                        <div class="code-section">
                            <div class="code-header">
                                📝 Original Code
                                <button class="copy-btn" onclick="copyToClipboard('originalCode')">Copy</button>
                            </div>
                            <textarea id="originalCode" class="code-textarea" placeholder="Paste your code here or load a sample..."></textarea>
                        </div>
                        
                        <div class="code-section">
                            <div class="code-header">
                                ⚡ Optimized Code
                                <button class="copy-btn" onclick="copyToClipboard('optimizedCode')">Copy</button>
                            </div>
                            <textarea id="optimizedCode" class="code-textarea" placeholder="Optimized code will appear here..." readonly></textarea>
                        </div>
                    </div>
                    
                    <div id="resultsSection" class="results-section" style="display: none;">
                        <div class="result-stats">
                            <div class="result-stat">
                                <div class="result-stat-value" id="originalSize">-</div>
                                <div class="result-stat-label">Original Size</div>
                            </div>
                            <div class="result-stat">
                                <div class="result-stat-value" id="optimizedSize">-</div>
                                <div class="result-stat-label">Optimized Size</div>
                            </div>
                            <div class="result-stat">
                                <div class="result-stat-value" id="reductionPercent">-</div>
                                <div class="result-stat-label">Size Reduction</div>
                            </div>
                            <div class="result-stat">
                                <div class="result-stat-value" id="bytesSaved">-</div>
                                <div class="result-stat-label">Bytes Saved</div>
                            </div>
                        </div>
                        
                        <div class="translation-section">
                            <div class="translation-header">🔄 Translation Back to Human-Readable:</div>
                            <textarea id="translatedCode" class="code-textarea" style="min-height: 200px;" readonly></textarea>
                        </div>
                    </div>
                </div>
            </div>
            
            <div class="sidebar">
                <h3 style="color: #4fc3f7; margin-bottom: 15px;">📊 Live Statistics</h3>
                
                <div class="stat-item">
                    <strong>Total Optimizations:</strong> <span id="sidebarTotal">0</span>
                </div>
                <div class="stat-item" style="margin: 10px 0;">
                    <strong>Average Reduction:</strong> <span id="sidebarAvg">0%</span>
                </div>
                <div class="stat-item">
                    <strong>Languages Processed:</strong> <span id="sidebarLangs">None</span>
                </div>
                
                <div class="recent-optimizations">
                    <h4 style="color: #81c784; margin-bottom: 10px;">🔥 Recent Optimizations</h4>
                    <div id="recentList">
                        <div style="color: #888; font-style: italic;">No optimizations yet</div>
                    </div>
                </div>
                
                <div style="margin-top: 30px;">
                    <h4 style="color: #4fc3f7; margin-bottom: 10px;">🎯 Features</h4>
                    <ul style="color: #aaa; font-size: 13px; line-height: 1.6;">
                        <li>✅ 60-95% size reduction</li>
                        <li>✅ Multi-language support</li>
                        <li>✅ Real-time translation</li>
                        <li>✅ Greek letter optimization</li>
                        <li>✅ Frequency-based symbols</li>
                        <li>✅ Complete functionality preservation</li>
                    </ul>
                </div>
            </div>
        </div>
    </div>

    <script>
        let currentOptimization = null;
        
        // Load sample code
        document.getElementById('loadSampleBtn').addEventListener('click', async () => {
            const language = document.getElementById('languageSelect').value;
            
            try {
                const response = await fetch(`/api/get-sample-code/${language}`);
                const data = await response.json();
                
                document.getElementById('originalCode').value = data.sample_code;
            } catch (error) {
                showError('Failed to load sample code: ' + error.message);
            }
        });
        
        // Optimize code
        document.getElementById('optimizeBtn').addEventListener('click', async () => {
            const code = document.getElementById('originalCode').value.trim();
            const language = document.getElementById('languageSelect').value;
            const strategy = document.getElementById('strategySelect').value;
            
            if (!code) {
                showError('Please enter some code to optimize');
                return;
            }
            
            const optimizeBtn = document.getElementById('optimizeBtn');
            optimizeBtn.disabled = true;
            optimizeBtn.innerHTML = '<span class="loading"></span> Optimizing...';
            
            try {
                const response = await fetch('/api/optimize-demo', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ code, language, strategy })
                });
                
                const result = await response.json();
                
                if (result.success) {
                    currentOptimization = result.result;
                    displayResults(result.result);
                    updateStats();
                    showSuccess(`Optimization complete! ${result.result.reduction_percent.toFixed(1)}% reduction achieved.`);
                } else {
                    showError('Optimization failed: ' + result.error);
                }
            } catch (error) {
                showError('Network error: ' + error.message);
            } finally {
                optimizeBtn.disabled = false;
                optimizeBtn.innerHTML = '🚀 OPTIMIZE';
            }
        });
        
        function displayResults(result) {
            // Show optimized code
            document.getElementById('optimizedCode').value = result.optimized_code;
            document.getElementById('translatedCode').value = result.translated_code;
            
            // Show statistics
            document.getElementById('originalSize').textContent = result.original_size + ' chars';
            document.getElementById('optimizedSize').textContent = result.optimized_size + ' chars';
            document.getElementById('reductionPercent').textContent = result.reduction_percent.toFixed(1) + '%';
            document.getElementById('bytesSaved').textContent = (result.original_size - result.optimized_size) + ' chars';
            
            // Show results section
            document.getElementById('resultsSection').style.display = 'block';
            
            // Scroll to results
            document.getElementById('resultsSection').scrollIntoView({ behavior: 'smooth' });
        }
        
        async function updateStats() {
            try {
                const response = await fetch('/api/stats');
                const stats = await response.json();
                
                // Update header stats
                document.getElementById('totalFiles').textContent = stats.total_files;
                document.getElementById('avgReduction').textContent = stats.average_reduction.toFixed(1) + '%';
                document.getElementById('languagesCount').textContent = stats.languages_processed.length;
                
                // Update sidebar stats
                document.getElementById('sidebarTotal').textContent = stats.total_files;
                document.getElementById('sidebarAvg').textContent = stats.average_reduction.toFixed(1) + '%';
                document.getElementById('sidebarLangs').textContent = stats.languages_processed.join(', ') || 'None';
                
                // Update recent optimizations
                const recentList = document.getElementById('recentList');
                if (stats.recent_optimizations.length > 0) {
                    recentList.innerHTML = stats.recent_optimizations.map(opt => `
                        <div class="optimization-item">
                            <span class="optimization-lang">${opt.language.toUpperCase()}</span>
                            <span class="optimization-reduction">${opt.reduction_percent.toFixed(1)}%</span>
                            <div style="margin-top: 5px; color: #888;">
                                ${opt.original_size} → ${opt.optimized_size} chars
                            </div>
                        </div>
                    `).join('');
                } else {
                    recentList.innerHTML = '<div style="color: #888; font-style: italic;">No optimizations yet</div>';
                }
            } catch (error) {
                console.error('Failed to update stats:', error);
            }
        }
        
        function copyToClipboard(elementId) {
            const element = document.getElementById(elementId);
            element.select();
            document.execCommand('copy');
            
            // Show feedback
            const btn = event.target;
            const originalText = btn.textContent;
            btn.textContent = 'Copied!';
            btn.style.background = '#4caf50';
            
            setTimeout(() => {
                btn.textContent = originalText;
                btn.style.background = '#666';
            }, 1500);
        }
        
        function showError(message) {
            const existing = document.querySelector('.error');
            if (existing) existing.remove();
            
            const errorDiv = document.createElement('div');
            errorDiv.className = 'error';
            errorDiv.textContent = message;
            
            document.querySelector('.demo-section').insertBefore(errorDiv, document.querySelector('.controls'));
            
            setTimeout(() => errorDiv.remove(), 5000);
        }
        
        function showSuccess(message) {
            const existing = document.querySelector('.success');
            if (existing) existing.remove();
            
            const successDiv = document.createElement('div');
            successDiv.className = 'success';
            successDiv.textContent = message;
            
            document.querySelector('.demo-section').insertBefore(successDiv, document.querySelector('.controls'));
            
            setTimeout(() => successDiv.remove(), 5000);
        }
        
        // Load initial stats
        updateStats();
        
        // Auto-refresh stats every 30 seconds
        setInterval(updateStats, 30000);
        
        // Load sample on page load
        window.addEventListener('load', () => {
            document.getElementById('loadSampleBtn').click();
        });
    </script>
</body>
</html>
'''

if __name__ == "__main__":
    print("🚀 Universal Code Optimizer - Showcase App")
    print("=" * 50)
    print("🌐 Starting showcase server...")
    print("🎯 Access at: http://localhost:5000")
    print("📱 Interactive demo with real-time optimization")
    print("=" * 50)
    
    app.run(host='0.0.0.0', port=5000, debug=True)
