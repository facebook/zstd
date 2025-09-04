import React, { useState, useEffect, useRef } from 'react';
import './MatrixTerminal.css';

const asciiArt = `
                                   ,::::                                        
                                   :::;;:::;                                    
                             ::;;;;;:,:::;::;   ,                               
                           ::::;::;;;::,:::;:::::                               
                          ::::,,,,,:,,,,,,,,,,,::::                             
                         ::::,,,,,:=iti+;,.. .,::::;                            
                      ,,:;:,,,.:iYVVXXRRXVt:  .,:::;:                           
                     ,::::,,..=IYVVVXXXRRXVY+  .,,,:::,                         
                     ::,,..,.;++iittVXXItiiit;.,.,::;:                          
                     ,,,,.:,,     ..:;:........,,.,,::,                         
                    ,,,,,...,.     .+YI:      ,,,.,::::,                        
                   ,,,,,...,+::,,,:+YXVt;,:,.:=, :,,.,,,                        
                   .,......=IYVVYYYIIVIIYYVVYYY; ..,,,,,                        
                    ..   ,:=IYVIIi+;=t;=+tIYXYI=;:..,,,,                        
                     ...  :=tI:;=+itYYIt+;;;itt=;. ...                          
                     ...  ::=+:+=tYVVYVVY++:;==:. ..,                           
         VVXRRRBMMMBBR+;::;;++=+++itttttiii====;:,,,;YI=i;++=                   
         IRBBBBW#WWMBXXVYYYYVXVVYIIYYVVXRBMW#WMMBRXXRRXVVYIYYt;                 
         +IYIYXBM###WMRRXVVVVVXVXRXXXRBBMW###WBRXYIttIIttiitt++                 
         =I+itIYXRMW###WWMBBRRBRBBBBMMWW###WMBRXVIti+++++===;+                  
         =+++ittIYVXRBMW###############WMMBRXVVXXXVt+===;;ii=;                  
         =i==+iitttIYVXXRRBMMWWWWWMMMBBBRRXVVYVYIYYVY=;;;;IYti                  
         ;===+++iItttIIYXVXXXRRRRRXXXXVXRRXYtIYVtitIXV+;;;tY=;                  
         ;=;==+ttIYYItittIIVVXXXXYIIIttYYXRXY+iitIIIYXVi=;+i;;                  
         ;;==+i+iii++i++itittItIItiiii+iiiittIIt++i=++iiYVVi=i                  
         ;=;;+i+=======++iti+i+++++++it+=+++=+==+i+;===;==+;=BB                 
         V;;;=====;=====+++++==+===++=+ii=;;;=;;==ii++=;;;;;+RXXV               
       XXX;;;;;==i;=;;;=i+==++;;=;;=====ti===;;=;;;++;;;;=iiVRRVYI              
      XXXR+=;;;;+tttti+==+==iIIi=;;;;;;+IYVI=;;;;;+tIti=;=IVXXXXVYYVVV          
  +tIYYXVVi;;;;==+YYVVi=;;++=+tii=;;;;===tiii;;;;;=;++++;;iXYXXXVVVVVXX         
VYIYYYYVXXXI;;;+IVVYIt+i++++=;;;;;;;;++i;=;;;;;;;;;;;;;;;;=iVVXRXXXXXXV         
VVVVVVVXVVXVIIt+==;;===+tIi++;;;;;;;;=i+=;;;;;;;;;==;;;;:;tRXVXXXXXXXXV         
XXXXRRRYVVitIt;;==;;;;;==+t++=;=;;;;;;==i=+=;:::::;;;;;;;=tIVXYXRRRRRXV         
XVXXXXRXYVI==;;;;;;;;;;+i+YIii==+====;;;t+tti+=;::::::;;=itVXXVVVVXRRVt         
RRRRXVVXYXXYYIti;;;;;;;=YtIXVVIYti+;+;;;XitIIItt=:::::;=++iIYVXRRXRXXVI         
RXXYVVVXRXI+;==;;;;;;=;;=;;++=+it++==;+==;;;;;;;:;;;;;;;;+++ItIVXXVVYtXB        
RRRXXXXVVI+;;;;;;;;;;=+++=+==i++=+i+=i++Y=;;;;::::::;===;;;;VYIitYVVYt+V+=      
XXXXXXXItYi;;;;;;;;;;;;;=;===+==+=+t==iii;;;;;;::;=;:::::;:=VVVitIVVYi=::;;     
YVXXVVIiYVt;;;;;;=;::::;=;=+++==+=+t++Iti+;;:::::;;+=;:;:::+VXVitYVVYI=;:::;    
YVVVYYitXRt;:::;+itt+=:::;;=iIItt;=+=+Vttti+;::::==ii+=::::iY:=;+tYYYI=:,::::   
IYVVYIi=IX+:::;;;;+=+====+++itItIIIIIYVIIYIYIIIIIIIYIIItIIIYY ;==+IIIt=:,,,::   
tYYYIt+;==:::+XRBBMWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWMMMBMMMX  ;=+ittti+;;:,:;   
iIIIti+;;     RBMBMWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWMMBBMMB   ;++iii==;;:,:    
+tttii+=      RRBMMWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWMMMRMMB    ===;==;;;:::    
=++++==+      RRBMBWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWMBMRBMB      ,::::::;:     
`;

interface MatrixTerminalProps {
  title?: string;
  subtitle?: string;
  showAscii?: boolean;
}

const MatrixTerminal: React.FC<MatrixTerminalProps> = ({ 
  title = "UNIVERSAL CODE OPTIMIZER", 
  subtitle = "EXTREME OPTIMIZATION SYSTEM",
  showAscii = true 
}) => {
  const [displayText, setDisplayText] = useState('');
  const [currentIndex, setCurrentIndex] = useState(0);
  const [showCursor, setShowCursor] = useState(true);
  const terminalRef = useRef<HTMLDivElement>(null);

  const fullText = `
> Initializing Universal Code Optimizer...
> Loading extreme optimization protocols...
> Greek symbol generation: α β γ δ ε ζ η θ ι κ λ μ ν ξ ο π ρ σ τ υ φ χ ψ ω
> Multi-language detection: READY
> Translation server: READY
> Safety protocols: ACTIVE
> 
> SYSTEM STATUS: READY FOR EXTREME OPTIMIZATION
> Expected reduction: 60-95%
> Human legibility: REMOVED
> Functionality: PRESERVED
>
> Welcome to the future of code optimization...
`;

  useEffect(() => {
    if (currentIndex < fullText.length) {
      const timeout = setTimeout(() => {
        setDisplayText(fullText.slice(0, currentIndex + 1));
        setCurrentIndex(currentIndex + 1);
      }, 50);
      return () => clearTimeout(timeout);
    }
  }, [currentIndex, fullText]);

  useEffect(() => {
    const cursorInterval = setInterval(() => {
      setShowCursor(prev => !prev);
    }, 500);
    return () => clearInterval(cursorInterval);
  }, []);

  return (
    <div className="matrix-terminal">
      {showAscii && (
        <div className="ascii-art">
          <pre>{asciiArt}</pre>
        </div>
      )}
      
      <div className="terminal-header">
        <h1 className="terminal-title">{title}</h1>
        <h2 className="terminal-subtitle">{subtitle}</h2>
      </div>
      
      <div className="terminal-content" ref={terminalRef}>
        <div className="terminal-output">
          <pre>{displayText}</pre>
          {showCursor && <span className="cursor">█</span>}
        </div>
      </div>
      
      <div className="terminal-stats">
        <div className="stat-item">
          <span className="stat-label">STATUS:</span>
          <span className="stat-value online">ONLINE</span>
        </div>
        <div className="stat-item">
          <span className="stat-label">MODE:</span>
          <span className="stat-value">EXTREME</span>
        </div>
        <div className="stat-item">
          <span className="stat-label">REDUCTION:</span>
          <span className="stat-value">95%</span>
        </div>
      </div>
    </div>
  );
};

export default MatrixTerminal;
