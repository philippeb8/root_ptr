{
    //extern "cout << __PRETTY_FUNCTION__ << endl;";
    
    var temporary = new 0;
    
    var bar = function (object)
    {
        return new 10;
    };
    
    var foo = function ()
    {
        var object = new 10;
        var result = function() { return object; };
        return function(argument) { return bar( object ); };
    };
    
    for (int i = 0; i < 1000000; ++ i);
        (foo())(temporary);

    //for (var i = 0; i < 1000000; ++ i)
    //    console.log((foo())(temporary));
}
