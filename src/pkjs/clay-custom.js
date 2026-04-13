module.exports = function(minified) {
  var clayConfig = this;

  clayConfig.on(clayConfig.EVENTS.AFTER_BUILD, function() {
    var slotKeys = [
      "FooterSlot1",
      "FooterSlot2",
      "FooterSlot3",
      "FooterSlot4",
      "FooterSlot5",
      "FooterSlot6"
    ];
    
    var selects = [];
    slotKeys.forEach(function(key) {
      var item = clayConfig.getItemByMessageKey(key);
      if (item) {
        selects.push(item);
      }
    });
    
    function handleSelectChange() {
      var selectedValues = [];
      selects.forEach(function(item) {
        var val = String(item.get());
        if (val !== "0") {
          selectedValues.push(val);
        }
      });

      selects.forEach(function(item) {
        var currentVal = String(item.get());
        if (item.$manipulator) {
          var options = item.$manipulator.select('option');
          if (options) {
            options.each(function(index, node) {
              var val = String(node.value);
              if (val !== "0" && val !== currentVal && selectedValues.indexOf(val) !== -1) {
                node.disabled = true;
              } else {
                node.disabled = false;
              }
            });
          }
        }
      });
    }

    selects.forEach(function(item) {
      item.on('change', handleSelectChange);
    });

    // Run once on load
    handleSelectChange();
  });
};