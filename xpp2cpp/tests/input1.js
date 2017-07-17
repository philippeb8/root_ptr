{
    //extern "cout << __PRETTY_FUNCTION__ << endl;";
    
    var temporary = 0;
    
    var bar = function (object)
    {
        return 10;
    };
    
    var foo = function ()
    {
        var object;
        var result = function() { return object; };
        return function(argument) { return bar( object ); };
    };
    
    extern "for (int i = 0; i < 1000000; ++ i)";
        extern "{";
        extern "cout << "; (foo())(temporary);
        extern "cout << endl;";
        extern "}";
        
    //for (var i = 0; i < 1000000; ++ i)
    //    console.log((foo())(temporary));
}
