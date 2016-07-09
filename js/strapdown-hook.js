$(document).ready(function() {
	var strapdown = function() {
			var script = document.createElement('script');
			script.src = "strapdown/v/0.2/strapdown.js";
			document.body.appendChild(script);
	};

	var xmpEl = $("xmp:first");
	if(xmpEl) {
		var xmpSrc = xmpEl.attr("src");
		if(xmpSrc) {
			$.ajax({
				url : xmpSrc,
				dataType : "text",
				success : function(data) {
					xmpEl.text(data);
					strapdown();
				}
			});
		} else {
			strapdown();
		}
	}
});
