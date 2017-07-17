{
    var temporary = new int(0);
    
    var bar = function ()
    {
        return new int(10);
    };
    
    var foo = function ()
    {
        var object = new int(11);
        return function(argument) { return argument; };
    };
    
    for (int i = 0; i < 1000000; ++ i)
        cout << (foo())(temporary) << endl;

    //for (var i = 0; i < 1000000; ++ i)
    //    console.log((foo())(temporary));
}
