import React, { useState, useEffect } from 'react';
import axios from 'axios';
import './CodeOptimizer.css';

interface OptimizationResult {
  original_code: string;
  optimized_code: string;
  translated_code: string;
  language: string;
  strategy: string;
  original_size: number;
  optimized_size: number;
  reduction_percent: number;
  timestamp: number;
}

interface Stats {
  total_files: number;
  average_reduction: number;
  languages_processed: string[];
  recent_optimizations: OptimizationResult[];
}

const CodeOptimizer: React.FC = () => {
  const [originalCode, setOriginalCode] = useState('');
  const [optimizedCode, setOptimizedCode] = useState('');
  const [translatedCode, setTranslatedCode] = useState('');
  const [language, setLanguage] = useState('c');
  const [strategy, setStrategy] = useState('memory_optimized');
  const [isOptimizing, setIsOptimizing] = useState(false);
  const [result, setResult] = useState<OptimizationResult | null>(null);
  const [stats, setStats] = useState<Stats | null>(null);
  const [error, setError] = useState('');
  const [success, setSuccess] = useState('');

  const API_BASE = 'http://localhost:5000/api';

  useEffect(() => {
    loadStats();
  }, []);

  const loadStats = async () => {
    try {
      const response = await axios.get(`${API_BASE}/stats`);
      setStats(response.data);
    } catch (error) {
      console.error('Failed to load stats:', error);
    }
  };

  const loadSampleCode = async () => {
    try {
      const response = await axios.get(`${API_BASE}/get-sample-code/${language}`);
      setOriginalCode(response.data.sample_code);
      setError('');
    } catch (error) {
      setError('Failed to load sample code');
    }
  };

  const optimizeCode = async () => {
    if (!originalCode.trim()) {
      setError('Please enter some code to optimize');
      return;
    }

    setIsOptimizing(true);
    setError('');
    setSuccess('');

    try {
      const response = await axios.post(`${API_BASE}/optimize-demo`, {
        code: originalCode,
        language,
        strategy
      });

      if (response.data.success) {
        const optimizationResult = response.data.result;
        setResult(optimizationResult);
        setOptimizedCode(optimizationResult.optimized_code);
        setTranslatedCode(optimizationResult.translated_code);
        setSuccess(`Optimization complete! ${optimizationResult.reduction_percent.toFixed(1)}% reduction achieved.`);
        loadStats(); // Refresh stats
      } else {
        setError('Optimization failed: ' + response.data.error);
      }
    } catch (error) {
      setError('Network error: ' + (error as any).message);
    } finally {
      setIsOptimizing(false);
    }
  };

  const copyToClipboard = (text: string, type: string) => {
    navigator.clipboard.writeText(text).then(() => {
      setSuccess(`${type} copied to clipboard!`);
      setTimeout(() => setSuccess(''), 2000);
    });
  };

  const getReductionColor = (reduction: number) => {
    if (reduction >= 90) return '#00ff00';
    if (reduction >= 70) return '#00ffff';
    if (reduction >= 50) return '#ffff00';
    return '#ff6600';
  };

  return (
    <div className="code-optimizer">
      <div className="optimizer-header">
        <h2>🔥 Live Code Optimization</h2>
        <p>Transform your code into ultra-optimized Greek symbols</p>
      </div>

      {/* Controls */}
      <div className="optimizer-controls">
        <div className="control-group">
          <label>Programming Language:</label>
          <select 
            value={language} 
            onChange={(e) => setLanguage(e.target.value)}
            className="control-select"
          >
            <option value="c">C</option>
            <option value="javascript">JavaScript</option>
            <option value="python">Python</option>
          </select>
        </div>

        <div className="control-group">
          <label>Optimization Strategy:</label>
          <select 
            value={strategy} 
            onChange={(e) => setStrategy(e.target.value)}
            className="control-select"
          >
            <option value="memory_optimized">Memory Optimized (α,β,γ)</option>
            <option value="speed_optimized">Speed Optimized</option>
            <option value="hybrid">Hybrid</option>
          </select>
        </div>

        <div className="control-group">
          <label>Sample Code:</label>
          <button 
            onClick={loadSampleCode}
            className="control-button secondary"
            disabled={isOptimizing}
          >
            Load Sample
          </button>
        </div>

        <div className="control-group">
          <label>&nbsp;</label>
          <button 
            onClick={optimizeCode}
            className="control-button primary"
            disabled={isOptimizing || !originalCode.trim()}
          >
            {isOptimizing ? (
              <>
                <span className="spinner"></span>
                Optimizing...
              </>
            ) : (
              '🚀 OPTIMIZE'
            )}
          </button>
        </div>
      </div>

      {/* Messages */}
      {error && <div className="message error">{error}</div>}
      {success && <div className="message success">{success}</div>}

      {/* Code Editors */}
      <div className="code-container">
        <div className="code-section">
          <div className="code-header">
            <span>📝 Original Code</span>
            <button 
              className="copy-btn"
              onClick={() => copyToClipboard(originalCode, 'Original code')}
            >
              Copy
            </button>
          </div>
          <textarea
            value={originalCode}
            onChange={(e) => setOriginalCode(e.target.value)}
            placeholder="Paste your code here or load a sample..."
            className="code-textarea"
          />
        </div>

        <div className="code-section">
          <div className="code-header">
            <span>⚡ Optimized Code</span>
            <button 
              className="copy-btn"
              onClick={() => copyToClipboard(optimizedCode, 'Optimized code')}
              disabled={!optimizedCode}
            >
              Copy
            </button>
          </div>
          <textarea
            value={optimizedCode}
            placeholder="Optimized code will appear here..."
            className="code-textarea"
            readOnly
          />
        </div>
      </div>

      {/* Results */}
      {result && (
        <div className="results-section">
          <div className="result-stats">
            <div className="result-stat">
              <div className="stat-value">{result.original_size}</div>
              <div className="stat-label">Original Size (chars)</div>
            </div>
            <div className="result-stat">
              <div className="stat-value">{result.optimized_size}</div>
              <div className="stat-label">Optimized Size (chars)</div>
            </div>
            <div className="result-stat">
              <div 
                className="stat-value"
                style={{ color: getReductionColor(result.reduction_percent) }}
              >
                {result.reduction_percent.toFixed(1)}%
              </div>
              <div className="stat-label">Size Reduction</div>
            </div>
            <div className="result-stat">
              <div className="stat-value">
                {result.original_size - result.optimized_size}
              </div>
              <div className="stat-label">Characters Saved</div>
            </div>
          </div>

          {/* Translation */}
          <div className="translation-section">
            <div className="translation-header">
              <span>🔄 Translation Back to Human-Readable:</span>
              <button 
                className="copy-btn"
                onClick={() => copyToClipboard(translatedCode, 'Translated code')}
              >
                Copy
              </button>
            </div>
            <textarea
              value={translatedCode}
              className="code-textarea translation-textarea"
              readOnly
            />
          </div>
        </div>
      )}

      {/* Live Stats */}
      {stats && (
        <div className="live-stats">
          <h3>📊 Live Statistics</h3>
          <div className="stats-grid">
            <div className="stat-card">
              <div className="stat-card-value">{stats.total_files}</div>
              <div className="stat-card-label">Total Optimizations</div>
            </div>
            <div className="stat-card">
              <div 
                className="stat-card-value"
                style={{ color: getReductionColor(stats.average_reduction) }}
              >
                {stats.average_reduction.toFixed(1)}%
              </div>
              <div className="stat-card-label">Average Reduction</div>
            </div>
            <div className="stat-card">
              <div className="stat-card-value">{stats.languages_processed.length}</div>
              <div className="stat-card-label">Languages Processed</div>
            </div>
          </div>

          {stats.recent_optimizations.length > 0 && (
            <div className="recent-optimizations">
              <h4>🔥 Recent Optimizations</h4>
              {stats.recent_optimizations.map((opt, index) => (
                <div key={index} className="optimization-item">
                  <div className="opt-lang">{opt.language.toUpperCase()}</div>
                  <div className="opt-reduction" style={{ color: getReductionColor(opt.reduction_percent) }}>
                    {opt.reduction_percent.toFixed(1)}%
                  </div>
                  <div className="opt-details">
                    {opt.original_size} → {opt.optimized_size} chars
                  </div>
                </div>
              ))}
            </div>
          )}
        </div>
      )}
    </div>
  );
};

export default CodeOptimizer;
