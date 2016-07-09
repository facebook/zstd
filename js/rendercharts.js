(function(window, document) {
  // http://stackoverflow.com/a/1414175/2132223
  var stringToBoolean1 = function (str) {
    switch(str.toLowerCase()) {
      case "true": case "yes": case "1": return true;
      case "false": case "no": case "0": case null: return false;
      default: return null;
    }
  };

  var stringToBoolean = function (str) {
    var b = stringToBoolean1(str);
    if(b === null) {
      b = Boolean(str);
    }
    return b;
  };

  // http://stackoverflow.com/a/4981700/2132223
  var getNestedObjectByName = function (strObjName) {
    var parts = strObjName.split(".");
    for (var i = 0, len = parts.length, obj = window; i < len; ++i) {
      obj = obj[parts[i]];
    }
    return obj;
  };

  // Copyright 2012-2013 Daniel Tillin
  //
  // Permission is hereby granted, free of charge, to any person obtaining
  // a copy of this software and associated documentation files (the
  // "Software"), to deal in the Software without restriction, including
  // without limitation the rights to use, copy, modify, merge, publish,
  // distribute, sublicense, and/or sell copies of the Software, and to
  // permit persons to whom the Software is furnished to do so, subject to
  // the following conditions:
  //
  // The above copyright notice and this permission notice shall be
  // included in all copies or substantial portions of the Software.
  // 
  // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
  // EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
  // MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
  // NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
  // LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
  // OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
  // WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  //
  // csvToArray v2.1 (Unminifiled for development)
  //
  // For documentation visit:
  // http://code.google.com/p/csv-to-array/
  //
  var csvToArray = function (csvStr, o) {
      var od = {
          'fSep': ',',
          'rSep': '\r\n',
          'quot': '"',
          'head': false,
          'trim': false
      }
      if (o) {
          for (var i in od) {
              if (!o[i]) o[i] = od[i];
          }
      } else {
          o = od;
      }
      var a = [
          ['']
      ];
      for (var r = f = p = q = 0; p < csvStr.length; p++) {
          switch (c = csvStr.charAt(p)) {
          case o.quot:
              if (q && csvStr.charAt(p + 1) == o.quot) {
                  a[r][f] += o.quot;
                  ++p;
              } else {
                  q ^= 1;
              }
              break;
          case o.fSep:
              if (!q) {
                  if (o.trim) {
                      a[r][f] = a[r][f].replace(/^\s\s*/, '').replace(/\s\s*$/, '');
                  }
                  a[r][++f] = '';
              } else {
                  a[r][f] += c;
              }
              break;
          case o.rSep.charAt(0):
              if (!q && (!o.rSep.charAt(1) || (o.rSep.charAt(1) && o.rSep.charAt(1) == csvStr.charAt(p + 1)))) {
                  if (o.trim) {
                      a[r][f] = a[r][f].replace(/^\s\s*/, '').replace(/\s\s*$/, '');
                  }
                  a[++r] = [''];
                  a[r][f = 0] = '';
                  if (o.rSep.charAt(1)) {
                      ++p;
                  }
              } else {
                  a[r][f] += c;
              }
              break;
          default:
              a[r][f] += c;
          }
      }
      if (o.head) {
          a.shift()
      }
      if (a[a.length - 1].length < a[0].length) {
          a.pop()
      }
      return a;
  };
  // end of http://code.google.com/p/csv-to-array/

  var estimateColumnType = function (csvAsArray, columnIndex) {
    var isColumnBoolean = function (csvAsArray, columnIndex) {
      for(var row = 0, nRow = csvAsArray.length; row < nRow; ++row) {
        if(stringToBoolean1(csvAsArray[row][columnIndex]) === null) {
          return false;
        }
      }
      return true;
    };

    var isColumnNumber = function (csvAsArray, columnIndex) {
      // http://stackoverflow.com/a/1830844/2132223
      var isNumber = function(n) {
        return !isNaN(parseFloat(n)) && isFinite(n);
      }
      for(var row = 0, nRow = csvAsArray.length; row < nRow; ++row) {
        if(! isNumber(csvAsArray[row][columnIndex])) {
          return false;
        }
      }
      return true;
    };

    if(isColumnBoolean(csvAsArray, columnIndex)) {
      return "boolean";
    }

    if(isColumnNumber(csvAsArray, columnIndex)) {
      return "number";
    }

    return "string";
  };

  var drawTable = function (gvCtor, gvOptions, csvUrl, csvEl) {
    var r = new XMLHttpRequest();
    r.open("GET", csvUrl, true);
    r.onreadystatechange = function() {
      if (this.readyState === 4 && this.status == 200) {
        var csvAsArray = csvToArray(this.responseText);

        var gvdt = new google.visualization.DataTable()
        var csvHeaderLine = csvAsArray.shift();
        for(var i = 0; i < csvHeaderLine.length; i++) {
/*
          var csvHeaderElement = csvHeaderLine[i].split(":");
          var csvHeaderName = "?";
          var csvHeaderType = "string";
          if(csvHeaderElement.length > 0) {
            csvHeaderName = csvHeaderElement[0];
            if(csvHeaderElement.length > 1) {
              csvHeaderType = csvHeaderElement[1].trim();
            }
          }
          gvdt.addColumn(csvHeaderType, csvHeaderName);
          for(var j = 0; j < csvAsArray.length; j++) {
            switch(csvHeaderType) {
              default:
              case "string":  csvAsArray[j][i] = String(csvAsArray[j][i]); break;
              case "number":  csvAsArray[j][i] = Number(csvAsArray[j][i]); break;
              case "boolean": csvAsArray[j][i] = stringToBoolean(csvAsArray[j][i]); break;
            }
          }
*/
			var csvHeaderType = estimateColumnType(csvAsArray, i);
//			alert("column" + i + " = " + csvHeaderType);
          gvdt.addColumn(csvHeaderType, csvHeaderLine[i]);
          for(var j = 0; j < csvAsArray.length; j++) {
            switch(csvHeaderType) {
              default:
              case "string":  csvAsArray[j][i] = String(csvAsArray[j][i]); break;
              case "number":  csvAsArray[j][i] = Number(csvAsArray[j][i]); break;
              case "boolean": csvAsArray[j][i] = stringToBoolean(csvAsArray[j][i]); break;
            }
          }
		}
        gvdt.addRows(csvAsArray);
        var ctor = getNestedObjectByName(gvCtor);
        var table = new ctor (csvEl);
        table.draw(gvdt, gvOptions);
      }
    }
    r.send();
  };

  var foreachCsvElement = function(cb) {
    var divEls = document.getElementsByTagName("div");
    for (var i = 0, n = divEls.length; i < n; i++) {
      var divEl = divEls[i];
      var propsAttr = divEl.getAttribute("data-csv-props");
      if(propsAttr !== null) {
        cb(divEl, JSON.parse(propsAttr));
      }
    }
  };

  var packages = function() {
    var gvPackageSet = [];
    foreachCsvElement(function(csvEl, props) {
      var gvPackageName = props["csvGvPackage"];
      if(! gvPackageSet.hasOwnProperty(gvPackageName)) {
        gvPackageSet.push(gvPackageName);
      }
    });
    return gvPackageSet;
  }();

  google.load("visualization", "1", {"packages": packages});
  google.setOnLoadCallback(function() {
    foreachCsvElement(function(csvEl, props) {
      var srcAttr = props["csvSrc"];
      var gvCtorName = props["csvGvType"];
      drawTable(gvCtorName, props, srcAttr, csvEl);
    });
  });
})(window, document);
