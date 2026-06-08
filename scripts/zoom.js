document.addEventListener("DOMContentLoaded", function () {
    document.querySelectorAll('object[type="image/svg+xml"]').forEach(function (obj) {
        var img = document.createElement("img");
        img.src = obj.getAttribute("data");
        img.setAttribute("width", obj.getAttribute("width"));
        img.classList.add("dox-svg");
        obj.parentNode.replaceChild(img, obj);
    });

    var lens = document.createElement("div");
    lens.id = "dox-lens";
    document.body.appendChild(lens);

    var zoomBox = document.createElement("div");
    zoomBox.id = "dox-zoombox";
    document.body.appendChild(zoomBox);

    var currentImg = null;
    var ZOOM = 1;

    function moveLens(e) {
        if (!currentImg) return;

        var rect = currentImg.getBoundingClientRect();
        var lensW = lens.offsetWidth;
        var lensH = lens.offsetHeight;
        var x = e.clientX - rect.left;
        var y = e.clientY - rect.top;

        lens.style.left = (rect.left + x - lensW / 2 + window.scrollX) + "px";
        lens.style.top  = (rect.top  + y - lensH / 2 + window.scrollY) + "px";

        var sampleX = Math.max(0, Math.min(rect.width, x));
        var sampleY = Math.max(0, Math.min(rect.height, y));
        var zoomW = zoomBox.offsetWidth;
        var zoomH = zoomBox.offsetHeight;

        zoomBox.style.backgroundSize     = (currentImg.naturalWidth * ZOOM) + "px " + (currentImg.naturalHeight * ZOOM) + "px";
        zoomBox.style.backgroundPosition = (-((sampleX / rect.width)  * currentImg.naturalWidth  * ZOOM - zoomW / 2)) + "px " +
                                           (-((sampleY / rect.height) * currentImg.naturalHeight * ZOOM - zoomH / 2)) + "px";
    }

    function positionZoomBox() {
        if (!currentImg) return;
        var rect = currentImg.getBoundingClientRect();
        var boxW = zoomBox.offsetWidth;

        if (rect.right + 20 + boxW < window.innerWidth) {
            zoomBox.style.left = (rect.right + window.scrollX + 20) + "px";
        } else {
            zoomBox.style.left = (rect.left + window.scrollX - boxW - 20) + "px";
        }

        zoomBox.style.top    = (rect.top + window.scrollY) + "px";
        zoomBox.style.height = Math.min(400, rect.height) + "px";
    }

    document.addEventListener("mousemove", function (e) {
        if (!currentImg) return;
        moveLens(e);
    });

    document.addEventListener("mouseover", function (e) {
        if (!e.target.classList.contains("dox-svg")) return;
        currentImg = e.target;
        zoomBox.style.backgroundImage = "url('" + currentImg.src + "')";
        lens.style.display    = "block";
        zoomBox.style.display = "block";
        positionZoomBox();
        moveLens(e);
    });

    document.addEventListener("mouseout", function (e) {
        if (!e.target.classList.contains("dox-svg")) return;
        currentImg = null;
        lens.style.display    = "none";
        zoomBox.style.display = "none";
    });

    window.addEventListener("scroll", function () {
        if (currentImg) positionZoomBox();
    });
});
