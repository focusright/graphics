function gotoNextPage() {
    let path = location.pathname;
    let filename = path.substring(path.lastIndexOf('/')+1);
    let first = filename.split('.')[0]
    let index = first.split('triangulation')[1];
    let nextIndex = parseInt(index) + 1;
    let nextIndexStr = nextIndex.toString();
    if(nextIndex < 10) { nextIndexStr = "0" + nextIndexStr; }
    location.href = './triangulation' + nextIndexStr + '.html';
}